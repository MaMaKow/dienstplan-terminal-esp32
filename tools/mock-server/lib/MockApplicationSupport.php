<?php

declare(strict_types=1);

/** Shared persistence, authentication, validation, and response helpers. */
trait MockApplicationSupport
{
    /** Load a fictional initial dataset on the first request. */
    private function loadState(): array
    {
        $directory = dirname($this->statePath);
        if (!is_dir($directory)) {
            mkdir($directory, 0770, true);
        }

        if (!is_file($this->statePath)) {
            $initialState = [
                'registrations' => [],
                'devices' => [
                    'dev_mock' => [
                        'device_id' => 'dev_mock',
                        'device_name' => 'Mock-Terminal',
                        'branch_id' => '42',
                        'status' => 'active',
                        'client_id' => 'device_mock',
                        'secret_hash' => password_hash('test-secret', PASSWORD_DEFAULT),
                        'created_at' => $this->now(),
                    ],
                ],
                'cards' => [
                    '04A1B2C3D4E5F6' => [
                        'card_uid' => '04A1B2C3D4E5F6',
                        'employee_id' => 'emp_123',
                        'branch_id' => '42',
                        'status' => 'active',
                        'state' => 'off_duty',
                        'display_name' => 'Erika Mustermann',
                    ],
                ],
                'access_tokens' => [],
                'employees' => [
                    'emp_123' => [
                        'id' => 'emp_123',
                        'display_name' => 'Erika Mustermann',
                        'branch_id' => '42',
                    ],
                ],
                'branches' => ['42' => true],
                'idempotency' => [],
                'limits' => [],
            ];

            file_put_contents(
                $this->statePath,
                json_encode($initialState, JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE),
                LOCK_EX
            );
            return $initialState;
        }

        $state = json_decode((string) file_get_contents($this->statePath), true);
        return is_array($state) ? $state : [];
    }

    /** Persist state after a request changes mock data. */
    private function save(): void
    {
        $temporaryPath = $this->statePath . '.tmp';
        $encodedState = json_encode(
            $this->state,
            JSON_PRETTY_PRINT | JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES
        );
        file_put_contents($temporaryPath, $encodedState, LOCK_EX);
        rename($temporaryPath, $this->statePath);
    }

    private function now(): string
    {
        return gmdate('Y-m-d\TH:i:s\Z');
    }

    private function id(string $prefix): string
    {
        return $prefix . '_' . strtoupper(bin2hex(random_bytes(8)));
    }

    private function header(string $name): string
    {
        $serverKey = 'HTTP_' . strtoupper(str_replace('-', '_', $name));
        return $_SERVER[$serverKey] ?? '';
    }

    /** Read the ordinary bearer scheme and support deliberate mock error tokens. */
    private function bearer(): string
    {
        $authorization = $this->header('Authorization');
        if (!preg_match('/^Bearer\s+(.+)$/i', $authorization, $matches)) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }
        if ($matches[1] === 'expired') {
            $this->fail('TOKEN_EXPIRED', 401);
        }
        if ($matches[1] === 'revoked') {
            $this->fail('DEVICE_REVOKED', 403);
        }

        return $matches[1];
    }

    private function admin(): void
    {
        $token = $this->bearer();
        if (!in_array($token, ['admin-token', 'provisioning-admin-token'], true)) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }
    }

    private function appUser(string $branchId): void
    {
        $token = $this->bearer();
        if (!in_array($token, ['user-token', 'app-user-token'], true)) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }
        if (!isset($this->state['branches'][$branchId])) {
            $this->fail('BRANCH_MISMATCH', 403);
        }
    }

    /** Resolve an issued access token to its active device. */
    private function device(): array
    {
        $token = $this->bearer();
        $tokenHash = hash('sha256', $token);
        $tokenRecord = $this->state['access_tokens'][$tokenHash] ?? null;
        if (!$tokenRecord || $tokenRecord['expires_at'] <= time()) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }

        $device = $this->state['devices'][$tokenRecord['device_id']] ?? null;
        if (!$device || $device['status'] !== 'active') {
            $this->fail('DEVICE_REVOKED', 403);
        }

        return $device;
    }

    /** Validate the Claim authorization scheme for registration polling/ack. */
    private function claim(string $registrationId): array
    {
        $authorization = $this->header('Authorization');
        if (!preg_match('/^Claim\s+(.+)$/i', $authorization, $matches)) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }

        $registration = $this->state['registrations'][$registrationId] ?? null;
        $claimHash = hash('sha256', $matches[1]);
        if (!$registration || !hash_equals($registration['claim_hash'], $claimHash)) {
            $this->fail('DEVICE_UNAUTHORIZED', 401);
        }
        if ($registration['status'] === 'pending' && strtotime($registration['expires_at']) <= time()) {
            $this->fail('REGISTRATION_EXPIRED', 410);
        }
        if ($registration['status'] === 'consumed') {
            $this->fail('CLAIM_CONSUMED', 410);
        }

        return $registration;
    }

    private function json(array $data, int $status = 200): never
    {
        http_response_code($status);
        header('Content-Type: application/json; charset=utf-8');
        echo json_encode($data, JSON_UNESCAPED_UNICODE | JSON_UNESCAPED_SLASHES);
        exit;
    }

    /** Send the contract error object, including booking-specific conflict data. */
    private function fail(
        string $code,
        int $status,
        ?string $message = null,
        mixed $details = null
    ): never {
        $messages = [
            'DEVICE_UNAUTHORIZED' => 'Das Terminal ist nicht angemeldet.',
            'TOKEN_EXPIRED' => 'Die Verbindung muss erneuert werden.',
            'DEVICE_REVOKED' => 'Dieses Terminal ist deaktiviert.',
            'CARD_UNKNOWN' => 'Diese Karte ist nicht zugeordnet.',
            'CARD_BLOCKED' => 'Diese Karte ist gesperrt.',
            'CARD_REVOKED' => 'Diese Karte ist nicht mehr gültig.',
            'ACTION_NOT_ALLOWED' => 'Diese Buchung ist aktuell nicht möglich.',
            'BRANCH_MISMATCH' => 'Die Buchung ist für dieses Terminal nicht möglich.',
            'CLAIM_CODE_INVALID' => 'Der Sicherheitscode ist ungültig.',
            'VALIDATION_ERROR' => 'Die Anfrage konnte nicht verarbeitet werden.',
            'REGISTRATION_EXPIRED' => 'Die Inbetriebnahme ist abgelaufen. Bitte erneut starten.',
            'CLAIM_CONSUMED' => 'Das Terminal wurde bereits eingerichtet.',
            'RATE_LIMITED' => 'Bitte kurz warten und erneut versuchen.',
            'SERVICE_UNAVAILABLE' => 'Der Server ist gerade nicht erreichbar.',
            'SERVER_ERROR' => 'Der Vorgang ist fehlgeschlagen. Bitte erneut versuchen.',
        ];

        $error = [
            'code' => $code,
            'userMessage' => $message ?? ($messages[$code] ?? 'Die Anfrage konnte nicht verarbeitet werden.'),
            'requestId' => $this->requestId,
            'details' => $details,
        ];

        // A rejected booking includes the current state so the terminal can explain the conflict.
        if ($status === 409 && $this->path === '/v1/terminal/bookings') {
            $state = $this->stateForRequest();
            $this->json([
                'result' => 'rejected',
                'error' => $error,
                'state' => $state,
                'allowed_actions' => $this->actions($state),
                'idempotency_key' => $this->body['idempotency_key'] ?? null,
            ], $status);
        }

        $this->json(['error' => $error], $status);
    }

    /** Generate stable local secrets unless the operator supplies them through the environment. */
    private function localSecret(string $environmentName, string $filename): string
    {
        $configuredSecret = getenv($environmentName);
        if ($configuredSecret !== false && $configuredSecret !== '') {
            return $configuredSecret;
        }

        $secretPath = dirname($this->statePath) . '/.' . $filename;
        if (!is_file($secretPath)) {
            file_put_contents($secretPath, bin2hex(random_bytes(32)), LOCK_EX);
            @chmod($secretPath, 0600);
        }

        return trim((string) file_get_contents($secretPath));
    }

    private function encryptionKey(): string
    {
        return hash('sha256', $this->localSecret('MOCK_CREDENTIALS_KEY', 'credentials.key'), true);
    }

    private function securityPepper(): string
    {
        return $this->localSecret('MOCK_SECURITY_PEPPER', 'security.pepper');
    }

    /** Encrypt one-time credentials while they wait for the device's acknowledgement. */
    private function encryptSecret(string $secret): string
    {
        $nonce = random_bytes(12);
        $tag = '';
        $ciphertext = openssl_encrypt(
            $secret,
            'aes-256-gcm',
            $this->encryptionKey(),
            OPENSSL_RAW_DATA,
            $nonce,
            $tag
        );
        if ($ciphertext === false) {
            $this->fail('SERVER_ERROR', 500);
        }

        return base64_encode($nonce . $tag . $ciphertext);
    }

    private function decryptSecret(string $encodedSecret): string
    {
        $payload = base64_decode($encodedSecret, true);
        if ($payload === false || strlen($payload) < 28) {
            $this->fail('SERVER_ERROR', 500);
        }

        $plaintext = openssl_decrypt(
            substr($payload, 28),
            'aes-256-gcm',
            $this->encryptionKey(),
            OPENSSL_RAW_DATA,
            substr($payload, 0, 12),
            substr($payload, 12, 16)
        );
        if ($plaintext === false) {
            $this->fail('SERVER_ERROR', 500);
        }

        return $plaintext;
    }

    private function required(string ...$fields): void
    {
        foreach ($fields as $field) {
            if (!isset($this->body[$field]) || $this->body[$field] === '') {
                $this->fail('VALIDATION_ERROR', 400);
            }
        }
    }

    private function validUid(string $uid): bool
    {
        return (bool) preg_match('/^[0-9A-Fa-f]{8,20}$/', $uid) && strlen($uid) % 2 === 0;
    }

    private function actions(string $state): array
    {
        $actionsByState = [
            'off_duty' => ['shift_start'],
            'on_shift' => ['shift_end', 'break_start'],
            'on_break' => ['break_end'],
        ];

        return $actionsByState[$state] ?? [];
    }

    private function stateForRequest(): string
    {
        $uid = strtoupper((string) ($this->body['card_uid'] ?? ''));
        return $this->state['cards'][$uid]['state'] ?? 'off_duty';
    }
}
