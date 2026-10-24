<?php

declare(strict_types=1);

/** Handlers for card assignment and revocation from the smartphone app. */
trait MockAppCardRoutes
{
    /** Assign a card only if it has no existing assignment. */
    private function assignCard(): never
    {
        $this->required('card_uid', 'employee_id', 'branch_id');
        $branchId = (string) $this->body['branch_id'];
        $this->appUser($branchId);

        $cardUid = strtoupper((string) $this->body['card_uid']);
        $employeeId = (string) $this->body['employee_id'];
        if (!$this->validUid($cardUid)) {
            $this->fail('VALIDATION_ERROR', 400);
        }
        if (isset($this->state['cards'][$cardUid])) {
            $this->fail(
                'VALIDATION_ERROR',
                409,
                'Diese Karte ist bereits einer Person zugeordnet.',
                ['reason' => 'card_already_assigned']
            );
        }

        $employee = $this->state['employees'][$employeeId] ?? null;
        if (!$employee) {
            $this->fail('VALIDATION_ERROR', 400);
        }
        if ($employee['branch_id'] !== $branchId) {
            $this->fail('BRANCH_MISMATCH', 403);
        }

        $assignedAt = $this->now();
        $this->state['cards'][$cardUid] = [
            'card_uid' => $cardUid,
            'employee_id' => $employeeId,
            'branch_id' => $branchId,
            'status' => 'active',
            'state' => 'off_duty',
            'display_name' => $employee['display_name'],
            'assigned_at' => $assignedAt,
        ];
        $this->save();

        $this->json([
            'card' => [
                'card_uid' => $cardUid,
                'employee_id' => $employeeId,
                'branch_id' => $branchId,
                'status' => 'active',
                'assigned_at' => $assignedAt,
            ],
        ], 201);
    }

    /** Revoke a card in the branch available to the authenticated app user. */
    private function revokeCard(string $cardUid): never
    {
        $this->required('reason');
        $cardUid = strtoupper($cardUid);
        if (!$this->validUid($cardUid)) {
            $this->fail('VALIDATION_ERROR', 400);
        }

        $card = $this->state['cards'][$cardUid] ?? null;
        if (!$card) {
            $this->fail('CARD_UNKNOWN', 404, 'Diese Karte ist nicht registriert.');
        }
        $this->appUser($card['branch_id']);

        // The contract makes repeated revocation idempotent, so keep returning 200.
        $card['status'] = 'revoked';
        $this->state['cards'][$cardUid] = $card;
        $this->save();

        $this->json([
            'card_uid' => $cardUid,
            'status' => 'revoked',
            'revoked_at' => $this->now(),
        ]);
    }
}
