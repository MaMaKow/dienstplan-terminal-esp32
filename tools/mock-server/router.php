<?php

declare(strict_types=1);

require_once __DIR__ . '/lib/MockApplication.php';

try {
    (new MockApplication())->run();
} catch (Throwable $error) {
    http_response_code(500);
    header('Content-Type: application/json; charset=utf-8');
    echo json_encode(['error' => [
        'code' => 'SERVER_ERROR',
        'userMessage' => 'Der Vorgang ist fehlgeschlagen. Bitte erneut versuchen.',
        'requestId' => 'req_' . strtoupper(bin2hex(random_bytes(6))),
        'details' => null,
    ]], JSON_UNESCAPED_UNICODE);
}
