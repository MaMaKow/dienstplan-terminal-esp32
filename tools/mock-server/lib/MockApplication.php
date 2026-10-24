<?php

declare(strict_types=1);

require_once __DIR__ . '/MockApplicationSupport.php';
require_once __DIR__ . '/MockProvisioningRoutes.php';
require_once __DIR__ . '/MockTerminalRoutes.php';
require_once __DIR__ . '/MockAppCardRoutes.php';

/** Dispatches HTTP requests to the mock API's route groups. */
final class MockApplication
{
    use MockApplicationSupport;
    use MockProvisioningRoutes;
    use MockTerminalRoutes;
    use MockAppCardRoutes;

    private array $state;
    private string $statePath;
    private string $requestId;
    private string $method;
    private string $path;
    private array $body = [];

    public function __construct()
    {
        $this->statePath = __DIR__ . '/../data/state.json';
        $this->state = $this->loadState();
        $this->requestId = 'req_' . strtoupper(bin2hex(random_bytes(6)));
        $this->method = strtoupper($_SERVER['REQUEST_METHOD'] ?? 'GET');
        $this->path = parse_url($_SERVER['REQUEST_URI'] ?? '/', PHP_URL_PATH) ?: '/';

        // Token requests use form encoding; the remaining endpoints use JSON.
        $rawBody = file_get_contents('php://input');
        $contentType = strtolower($this->header('Content-Type'));
        if ($rawBody !== false && trim($rawBody) !== '' && str_contains($contentType, 'application/json')) {
            $decodedBody = json_decode($rawBody, true);
            if (!is_array($decodedBody)) {
                $this->fail('VALIDATION_ERROR', 400);
            }
            $this->body = $decodedBody;
        }
    }

    /** Match the request to a route and invoke its handler. */
    public function run(): void
    {
        if (($_SERVER['HTTP_X_MOCK_ERROR'] ?? '') === '503') {
            $this->fail('SERVICE_UNAVAILABLE', 503);
        }

        $method = $this->method;
        $path = $this->path;

        // Device provisioning and administration routes.
        if ($method === 'POST' && $path === '/v1/device-auth/token') {
            $this->token();
        }
        if ($method === 'POST' && $path === '/v1/device-registrations') {
            $this->register();
        }
        if ($method === 'GET' && $path === '/v1/admin/device-registrations') {
            $this->pendingList();
        }
        if (preg_match('~^/v1/admin/device-registrations/([^/]+)/approve$~', $path, $matches) && $method === 'POST') {
            $this->approve(rawurldecode($matches[1]));
        }
        if (preg_match('~^/v1/device-registrations/([^/]+)(?:/(ack))?$~', $path, $matches)) {
            $registrationId = rawurldecode($matches[1]);
            if ($method === 'GET' && empty($matches[2])) {
                $this->poll($registrationId);
            }
            if ($method === 'POST' && ($matches[2] ?? '') === 'ack') {
                $this->ack($registrationId);
            }
        }
        if ($method === 'POST' && $path === '/v1/admin/devices') {
            $this->createDevice();
        }
        if (preg_match('~^/v1/admin/devices/([^/]+)(?:/(credentials/rotate|revoke))?$~', $path, $matches)) {
            $deviceId = rawurldecode($matches[1]);
            $action = $matches[2] ?? '';
            if ($method === 'PATCH' && $action === '') {
                $this->renameDevice($deviceId);
            }
            if ($method === 'POST' && $action === 'credentials/rotate') {
                $this->rotate($deviceId);
            }
            if ($method === 'POST' && $action === 'revoke') {
                $this->revokeDevice($deviceId);
            }
        }

        // Terminal routes.
        if ($method === 'POST' && $path === '/v1/terminal/cards/resolve') {
            $this->resolveCard();
        }
        if ($method === 'POST' && $path === '/v1/terminal/bookings') {
            $this->book();
        }

        // Smartphone app routes.
        if ($method === 'POST' && $path === '/v1/app/cards/assign') {
            $this->assignCard();
        }
        if (preg_match('~^/v1/app/cards/([^/]+)/revoke$~', $path, $matches) && $method === 'POST') {
            $this->revokeCard(rawurldecode($matches[1]));
        }

        $this->fail('VALIDATION_ERROR', 404, 'Der Endpunkt wurde nicht gefunden.');
    }
}
