<?php

declare(strict_types=1);

/** Handlers for terminal card resolution and shift booking routes. */
trait MockTerminalRoutes
{
    /** Resolve a card and return current state plus only the allowed actions. */
    private function resolveCard(): never
    {
        $device = $this->device();
        $this->required('device_id', 'branch_id', 'card_uid');

        $cardUid = strtoupper((string) $this->body['card_uid']);
        if (!$this->validUid($cardUid)) {
            $this->fail('VALIDATION_ERROR', 400);
        }
        if (
            (string) $this->body['device_id'] !== $device['device_id']
            || (string) $this->body['branch_id'] !== $device['branch_id']
        ) {
            $this->fail('BRANCH_MISMATCH', 403);
        }

        $card = $this->state['cards'][$cardUid] ?? null;
        if (!$card) {
            $this->fail('CARD_UNKNOWN', 404);
        }
        if ($card['branch_id'] !== $device['branch_id']) {
            $this->fail('BRANCH_MISMATCH', 403);
        }
        if ($card['status'] === 'blocked') {
            $this->fail('CARD_BLOCKED', 403);
        }
        if ($card['status'] === 'revoked') {
            $this->fail('CARD_REVOKED', 403);
        }

        $allowedActions = $this->actions($card['state']);
        $labels = [
            'shift_start' => 'Dienstbeginn',
            'shift_end' => 'Dienstende',
            'break_start' => 'Pausenbeginn',
            'break_end' => 'Pausenende',
        ];
        $message = match ($card['state']) {
            'off_duty' => 'Dienstbeginn möglich.',
            'on_break' => 'Pausenende möglich.',
            default => 'Buchung möglich.',
        };

        $this->json([
            'card_status' => 'active',
            'employee' => [
                'id' => $card['employee_id'],
                'display_name' => $card['display_name'],
            ],
            'state' => $card['state'],
            'allowed_actions' => $allowedActions,
            'display' => [
                'title' => 'Guten Tag, ' . $card['display_name'],
                'message' => $message,
                'action_labels' => array_intersect_key($labels, array_flip($allowedActions)),
            ],
            'resolved_at' => $this->now(),
        ]);
    }

    /**
     * Validate current card state on every booking and cache the full response by key.
     * Reusing a key with a different action or card is rejected without changing state.
     */
    private function book(): never
    {
        $device = $this->device();
        $this->required('device_id', 'branch_id', 'card_uid', 'action', 'idempotency_key');

        $cardUid = strtoupper((string) $this->body['card_uid']);
        $idempotencyKey = (string) $this->body['idempotency_key'];
        if (
            !$this->validUid($cardUid)
            || strlen($idempotencyKey) < 1
            || strlen($idempotencyKey) > 128
        ) {
            $this->fail('VALIDATION_ERROR', 400);
        }
        if (
            (string) $this->body['device_id'] !== $device['device_id']
            || (string) $this->body['branch_id'] !== $device['branch_id']
        ) {
            $this->fail('BRANCH_MISMATCH', 403);
        }

        $keyId = $device['device_id'] . ':' . $idempotencyKey;
        $fingerprint = hash('sha256', json_encode([
            $cardUid,
            $device['branch_id'],
            $this->body['action'],
        ]));
        if (isset($this->state['idempotency'][$keyId])) {
            $savedRequest = $this->state['idempotency'][$keyId];
            if ($savedRequest['fingerprint'] !== $fingerprint) {
                $this->fail('VALIDATION_ERROR', 409);
            }
            $this->json($savedRequest['response']);
        }

        $card = $this->state['cards'][$cardUid] ?? null;
        if (!$card) {
            $this->fail('CARD_UNKNOWN', 404);
        }
        if ($card['branch_id'] !== $device['branch_id']) {
            $this->fail('BRANCH_MISMATCH', 403);
        }
        if ($card['status'] === 'blocked') {
            $this->fail('CARD_BLOCKED', 403);
        }
        if ($card['status'] === 'revoked') {
            $this->fail('CARD_REVOKED', 403);
        }

        $action = (string) $this->body['action'];
        $validActions = ['shift_start', 'shift_end', 'break_start', 'break_end'];
        if (!in_array($action, $validActions, true)) {
            $this->fail('VALIDATION_ERROR', 400);
        }

        $transitions = [
            'off_duty' => ['shift_start' => 'on_shift'],
            'on_shift' => [
                'shift_end' => 'off_duty',
                'break_start' => 'on_break',
            ],
            'on_break' => ['break_end' => 'on_shift'],
        ];
        $currentState = $card['state'];
        if (!isset($transitions[$currentState][$action])) {
            $this->fail('ACTION_NOT_ALLOWED', 409);
        }

        $newState = $transitions[$currentState][$action];
        $card['state'] = $newState;
        $this->state['cards'][$cardUid] = $card;

        $messages = [
            'shift_start' => 'Dienstbeginn wurde gebucht.',
            'shift_end' => 'Dienstende wurde gebucht.',
            'break_start' => 'Pausenbeginn wurde gebucht.',
            'break_end' => 'Pausenende wurde gebucht.',
        ];
        $response = [
            'result' => 'success',
            'booking_id' => $this->id('booking'),
            'action' => $action,
            'message' => $messages[$action],
            'state' => $newState,
            'allowed_actions' => $this->actions($newState),
            'completed_at' => $this->now(),
            'idempotency_key' => $idempotencyKey,
        ];

        $this->state['idempotency'][$keyId] = [
            'fingerprint' => $fingerprint,
            'response' => $response,
            'created' => time(),
        ];
        $this->save();
        $this->json($response);
    }
}
