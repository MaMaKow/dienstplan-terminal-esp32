<?php

declare(strict_types=1);

/** Handlers for device authentication, claiming, and administration routes. */
trait MockProvisioningRoutes
{
    /** Exchange a provisioned device's Basic credentials for a short-lived token. */
    private function token(): never
    {
        $authorization = $this->header('Authorization');
        if (!preg_match('/^Basic\s+(.+)$/i', $authorization, $matches)) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }

        $credentials = base64_decode($matches[1], true);
        if ($credentials === false || !str_contains($credentials, ':')) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }

        [$clientId, $clientSecret] = explode(':', $credentials, 2);
        $device = null;
        foreach ($this->state['devices'] as $candidate) {
            if (
                $candidate['client_id'] === $clientId
                && password_verify($clientSecret, $candidate['secret_hash'])
            ) {
                $device = $candidate;
                break;
            }
        }
        if (!$device || $device['status'] !== 'active') {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }

        parse_str((string) file_get_contents('php://input'), $form);
        if (
            ($form['grant_type'] ?? '') !== 'client_credentials'
            || ($form['scope'] ?? 'terminal') !== 'terminal'
        ) {
            $this->fail('VALIDATION_ERROR', 400);
        }

        // Store only a hash of the random bearer token; its value is returned once.
        $accessToken = bin2hex(random_bytes(32));
        $tokenHash = hash('sha256', $accessToken);
        $this->state['access_tokens'][$tokenHash] = [
            'device_id' => $device['device_id'],
            'expires_at' => time() + 900,
        ];
        $this->save();

        $this->json([
            'access_token' => $accessToken,
            'token_type' => 'Bearer',
            'expires_in' => 900,
            'scope' => 'terminal',
            'device_id' => $device['device_id'],
            'branch_id' => $device['branch_id'],
        ]);
    }

    /** Apply simple per-minute limits; exact thresholds are open in the contract. */
    private function rate(string $key, int $limit): void
    {
        $now = time();
        $recentEvents = array_values(array_filter(
            $this->state['limits'][$key] ?? [],
            static fn ($timestamp) => $timestamp > $now - 60
        ));

        if (count($recentEvents) >= $limit) {
            header('Retry-After: 60');
            $this->fail('RATE_LIMITED', 429);
        }

        $recentEvents[] = $now;
        $this->state['limits'][$key] = $recentEvents;
        $this->save();
    }

    /** Create a pending registration and disclose its claim token only in this response. */
    private function register(): never
    {
        $this->required('device_name', 'security_code');
        $deviceName = trim((string) $this->body['device_name']);
        $securityCode = (string) $this->body['security_code'];
        if ($deviceName === '' || strlen($deviceName) > 128 || !preg_match('/^\d{6}$/', $securityCode)) {
            $this->fail('VALIDATION_ERROR', 400);
        }

        $sourceIp = $_SERVER['REMOTE_ADDR'] ?? 'unknown';
        $this->rate('reg:' . $sourceIp, 10);
        $pendingCount = 0;
        foreach ($this->state['registrations'] as $registration) {
            if (
                $registration['status'] === 'pending'
                && strtotime($registration['expires_at']) > time()
            ) {
                $pendingCount++;
            }
        }
        if ($pendingCount >= 20) {
            $this->fail('RATE_LIMITED', 429);
        }

        $registrationId = $this->id('reg');
        $claimToken = bin2hex(random_bytes(32));
        $expiresAt = gmdate('Y-m-d\TH:i:s\Z', time() + 900);
        $this->state['registrations'][$registrationId] = [
            'registration_id' => $registrationId,
            'device_name' => $deviceName,
            'claim_hash' => hash('sha256', $claimToken),
            'security_hash' => hash_hmac('sha256', $securityCode, $this->securityPepper()),
            'status' => 'pending',
            'requested_at' => $this->now(),
            'expires_at' => $expiresAt,
        ];
        $this->save();

        $this->json([
            'registration_id' => $registrationId,
            'claim_token' => $claimToken,
            'status' => 'pending',
            'expires_at' => $expiresAt,
        ], 201);
    }

    /** Return active pending claims without exposing their display security codes. */
    private function pendingList(): never
    {
        $this->admin();
        $registrations = [];
        foreach ($this->state['registrations'] as $registration) {
            if (
                $registration['status'] !== 'pending'
                || strtotime($registration['expires_at']) <= time()
            ) {
                continue;
            }

            $registrations[] = [
                'registration_id' => $registration['registration_id'],
                'device_name' => $registration['device_name'],
                'status' => 'pending',
                'requested_at' => $registration['requested_at'],
                'expires_at' => $registration['expires_at'],
            ];
        }

        $this->json(['registrations' => $registrations]);
    }

    /** Approve a claim only when its display code and branch both validate. */
    private function approve(string $registrationId): never
    {
        $this->admin();
        $this->required('security_code', 'branch_id');
        $registration = $this->state['registrations'][$registrationId] ?? null;
        if (!$registration) {
            $this->fail('VALIDATION_ERROR', 404, 'Die Registrierung konnte nicht gefunden werden.');
        }
        if (
            $registration['status'] !== 'pending'
            || strtotime($registration['expires_at']) <= time()
        ) {
            $this->fail('REGISTRATION_EXPIRED', 410);
        }

        $providedCodeHash = hash_hmac(
            'sha256',
            (string) $this->body['security_code'],
            $this->securityPepper()
        );
        if (!hash_equals($registration['security_hash'], $providedCodeHash)) {
            $this->fail('CLAIM_CODE_INVALID', 403);
        }

        $branchId = (string) $this->body['branch_id'];
        if (!isset($this->state['branches'][$branchId])) {
            $this->fail('BRANCH_MISMATCH', 403);
        }

        $deviceId = $this->id('dev');
        $clientId = 'device_' . $deviceId;
        $clientSecret = bin2hex(random_bytes(24));
        $approvedAt = $this->now();
        $this->state['devices'][$deviceId] = [
            'device_id' => $deviceId,
            'device_name' => $registration['device_name'],
            'branch_id' => $branchId,
            'status' => 'active',
            'client_id' => $clientId,
            'secret_hash' => password_hash($clientSecret, PASSWORD_DEFAULT),
            'created_at' => $approvedAt,
        ];

        // Keep the secret encrypted until the device confirms it received the credentials.
        $registration['status'] = 'approved';
        $registration['device_id'] = $deviceId;
        $registration['branch_id'] = $branchId;
        $registration['approved_at'] = $approvedAt;
        $registration['client_id'] = $clientId;
        $registration['secret_ciphertext'] = $this->encryptSecret($clientSecret);
        $this->state['registrations'][$registrationId] = $registration;
        $this->save();

        $this->json([
            'registration_id' => $registrationId,
            'device_id' => $deviceId,
            'device_name' => $registration['device_name'],
            'branch_id' => $branchId,
            'status' => 'approved',
            'approved_at' => $approvedAt,
        ]);
    }

    /** Return status, and repeat the same pending credential delivery until ack. */
    private function poll(string $registrationId): never
    {
        $registration = $this->claim($registrationId);
        $this->rate('claim:' . $registrationId, 60);

        if ($registration['status'] === 'pending') {
            $this->json([
                'status' => 'pending',
                'expires_at' => $registration['expires_at'],
            ]);
        }
        if ($registration['status'] === 'approved') {
            $this->json([
                'status' => 'approved',
                'device_id' => $registration['device_id'],
                'branch_id' => $registration['branch_id'],
                'credentials' => [
                    'client_id' => $registration['client_id'],
                    'client_secret' => $this->decryptSecret($registration['secret_ciphertext']),
                ],
            ]);
        }

        $this->fail('CLAIM_CONSUMED', 410);
    }

    /** Erase the temporarily encrypted credential after device confirmation. */
    private function ack(string $registrationId): never
    {
        $registration = $this->claim($registrationId);
        if ($registration['status'] !== 'approved') {
            $this->fail('VALIDATION_ERROR', 400);
        }

        $registration['status'] = 'consumed';
        $registration['consumed_at'] = $this->now();
        unset($registration['secret_ciphertext']);
        $this->state['registrations'][$registrationId] = $registration;
        $this->save();

        $this->json([
            'status' => 'consumed',
            'device_id' => $registration['device_id'],
        ]);
    }

    /** Create an active device directly through the admin endpoint. */
    private function createDevice(): never
    {
        $this->admin();
        $this->required('device_name', 'branch_id');
        $deviceName = trim((string) $this->body['device_name']);
        $branchId = (string) $this->body['branch_id'];

        if ($deviceName === '' || strlen($deviceName) > 128) {
            $this->fail('VALIDATION_ERROR', 400);
        }
        if (!isset($this->state['branches'][$branchId])) {
            $this->fail('BRANCH_MISMATCH', 404);
        }
        foreach ($this->state['devices'] as $device) {
            if ($device['device_name'] === $deviceName) {
                $this->fail('VALIDATION_ERROR', 409);
            }
        }

        $deviceId = $this->id('dev');
        $clientId = $deviceName;
        $clientSecret = bin2hex(random_bytes(24));
        $createdAt = $this->now();
        $this->state['devices'][$deviceId] = [
            'device_id' => $deviceId,
            'device_name' => $deviceName,
            'branch_id' => $branchId,
            'status' => 'active',
            'client_id' => $clientId,
            'secret_hash' => password_hash($clientSecret, PASSWORD_DEFAULT),
            'created_at' => $createdAt,
        ];
        $this->save();

        $this->json([
            'device_id' => $deviceId,
            'device_name' => $deviceName,
            'branch_id' => $branchId,
            'status' => 'active',
            'credentials' => [
                'client_id' => $clientId,
                'client_secret' => $clientSecret,
            ],
            'created_at' => $createdAt,
        ], 201);
    }

    private function renameDevice(string $deviceId): never
    {
        $this->admin();
        $this->required('device_name');
        $device = $this->state['devices'][$deviceId] ?? null;
        if (!$device) {
            $this->fail('VALIDATION_ERROR', 404, 'Das Terminal konnte nicht gefunden werden.');
        }

        $deviceName = trim((string) $this->body['device_name']);
        if ($deviceName === '') {
            $this->fail('VALIDATION_ERROR', 400);
        }
        foreach ($this->state['devices'] as $otherDevice) {
            if ($otherDevice['device_id'] !== $deviceId && $otherDevice['device_name'] === $deviceName) {
                $this->fail('VALIDATION_ERROR', 409);
            }
        }

        $device['device_name'] = $deviceName;
        $this->state['devices'][$deviceId] = $device;
        $this->save();
        $this->json([
            'device_id' => $deviceId,
            'device_name' => $deviceName,
            'updated_at' => $this->now(),
        ]);
    }

    /** Rotate the secret and invalidate all previously issued access tokens. */
    private function rotate(string $deviceId): never
    {
        $this->admin();
        $device = $this->state['devices'][$deviceId] ?? null;
        if (!$device || $device['status'] !== 'active') {
            $this->fail('VALIDATION_ERROR', 404, 'Das Terminal konnte nicht gefunden werden.');
        }

        $clientSecret = bin2hex(random_bytes(24));
        $device['secret_hash'] = password_hash($clientSecret, PASSWORD_DEFAULT);
        $this->state['devices'][$deviceId] = $device;
        $this->removeDeviceTokens($deviceId);
        $this->save();

        $this->json([
            'device_id' => $deviceId,
            'client_id' => $device['client_id'],
            'client_secret' => $clientSecret,
            'rotated_at' => $this->now(),
        ]);
    }

    /** Revoke a device and its active tokens. Repeating the call remains successful. */
    private function revokeDevice(string $deviceId): never
    {
        $this->admin();
        $device = $this->state['devices'][$deviceId] ?? null;
        if (!$device) {
            $this->fail('VALIDATION_ERROR', 404, 'Das Terminal konnte nicht gefunden werden.');
        }

        $device['status'] = 'revoked';
        $this->state['devices'][$deviceId] = $device;
        $this->removeDeviceTokens($deviceId);
        $this->save();

        $this->json([
            'device_id' => $deviceId,
            'status' => 'revoked',
            'revoked_at' => $this->now(),
        ]);
    }

    /** Remove bearer tokens tied to a device after revocation or credential rotation. */
    private function removeDeviceTokens(string $deviceId): void
    {
        foreach ($this->state['access_tokens'] as $tokenHash => $token) {
            if ($token['device_id'] === $deviceId) {
                unset($this->state['access_tokens'][$tokenHash]);
            }
        }
    }
}
