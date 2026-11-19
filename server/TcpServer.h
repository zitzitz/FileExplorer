#pragma once

#include <winsock2.h>
#include <windows.h>
#include <cstdint>
#include "Protocol.h"
#include "lib/WString.h"

//   Многопоточный TCP-сервер на базе WinSock2 и пула потоков Windows.
 
//   Архитектура работы:
//   - Start() / Stop(): запускают слушающий сокет и цикл диспетчеризации WSAPoll. При поступлении
//     данных от клиента сокет временно извлекается из массива ожидания и передается в Thread Pool.
//   - WorkerCallback() / ProcessSingleRequest(): выполняются в потоке пула, вычитывают один пакет
//     запроса и возвращают сокет обратно через потокобезопасную очередь ReturnSocketToPoll().
//   - ProcessGetDirList(): сканирует запрошенную директорию, конвертирует имена в UTF-8 и отправляет ответ.

class TcpServer
{
public:
    static constexpr uint32_t MAX_CLIENTS = 256;

    TcpServer() noexcept;
    ~TcpServer() noexcept;

    TcpServer(const TcpServer&) = delete;
    TcpServer& operator=(const TcpServer&) = delete;

    bool Start(uint16_t port) noexcept;
    void Stop() noexcept;

private:
    static VOID CALLBACK WorkerCallback(PTP_CALLBACK_INSTANCE instance, PVOID context, PTP_WORK work) noexcept;
    bool ProcessSingleRequest(SOCKET clientSocket) noexcept;
    bool ProcessGetDirList(SOCKET clientSocket, uint32_t payloadSize) noexcept;

    void ReturnSocketToPoll(SOCKET sock) noexcept;
    void DrainReturnedSockets(WSAPOLLFD* pollFds, uint32_t& pollCount) noexcept;

    bool SendAll(SOCKET sock, const void* data, int size) const noexcept;
    bool RecvAll(SOCKET sock, void* buffer, int length) const noexcept;
    bool RecvUtf8Path(SOCKET sock, uint32_t byteLen, WString& outPath) const noexcept;
    bool SendErrorResponse(SOCKET sock, CommandId cmdId, StatusCode statusCode) const noexcept;

private:
    SOCKET _listenSocket   = INVALID_SOCKET;
    volatile LONG _isRunning      = 0;
    volatile LONG _activeWorkers  = 0;
    bool _wsaInitialized = false;

    PTP_POOL _threadPool = nullptr;
    TP_CALLBACK_ENVIRON _callbackEnv{};

    CRITICAL_SECTION _returnLock{};
    SOCKET _returnedSockets[MAX_CLIENTS]{};
    uint32_t _returnedCount = 0;
};