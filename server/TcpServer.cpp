#include "TcpServer.h"
#include "lib/UniqueHandle.h"
#include "FileScanner.h"

namespace {

struct ClientTask
{
    TcpServer* server;
    SOCKET     socket;
};

struct SocketDeleter {
    static SOCKET Invalid() noexcept { return INVALID_SOCKET; }
    static void Close(SOCKET s) noexcept { closesocket(s); }
};

struct ProcessHeapDeleter {
    static void Close(void* ptr) noexcept { HeapFree(GetProcessHeap(), 0, ptr); }
};

struct ThreadpoolWorkDeleter {
    static void Close(PTP_WORK work) noexcept { CloseThreadpoolWork(work); }
};

using UniqueSocket = UniqueHandle<SOCKET, SocketDeleter>;
using TpWorkHandle = UniqueHandle<PTP_WORK, ThreadpoolWorkDeleter>;

template <typename T>
using HeapPtr = UniqueHandle<T*, ProcessHeapDeleter>;

void ConfigureClientSocket(SOCKET s) noexcept
{
    DWORD recvTimeoutMs = 2000; // 2 сек на дочитывание начатого пакета
    DWORD sendTimeoutMs = 3000; // 3 сек на отправку ответа
    setsockopt(s, SOL_SOCKET, SO_RCVTIMEO, reinterpret_cast<const char*>(&recvTimeoutMs), sizeof(recvTimeoutMs));
    setsockopt(s, SOL_SOCKET, SO_SNDTIMEO, reinterpret_cast<const char*>(&sendTimeoutMs), sizeof(sendTimeoutMs));
}

} // namespace

TcpServer::TcpServer() noexcept
{
    InitializeCriticalSection(&_returnLock);
    InitializeThreadpoolEnvironment(&_callbackEnv);
}

TcpServer::~TcpServer() noexcept
{
    Stop();
    if (_threadPool)
    {
        CloseThreadpool(_threadPool);
        _threadPool = nullptr;
    }
    DestroyThreadpoolEnvironment(&_callbackEnv);
    DeleteCriticalSection(&_returnLock);

    if (_wsaInitialized)
    {
        WSACleanup();
        _wsaInitialized = false;
    }
}

bool TcpServer::Start(uint16_t port) noexcept
{
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0)
        return false;
    _wsaInitialized = true;

    _threadPool = CreateThreadpool(nullptr);
    if (!_threadPool)
        return false;

    SetThreadpoolThreadMaximum(_threadPool, 16);
    SetThreadpoolThreadMinimum(_threadPool, 2);
    SetThreadpoolCallbackPool(&_callbackEnv, _threadPool);

    UniqueSocket tempListen(socket(AF_INET, SOCK_STREAM, IPPROTO_TCP));
    if (!tempListen)
        return false;

    BOOL optVal = TRUE;
    setsockopt(tempListen.get(), SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&optVal), sizeof(optVal));

    sockaddr_in serverAddr{};
    serverAddr.sin_family      = AF_INET;
    serverAddr.sin_addr.s_addr = INADDR_ANY;
    serverAddr.sin_port        = htons(port);

    if (bind(tempListen.get(), reinterpret_cast<sockaddr*>(&serverAddr), sizeof(serverAddr)) == SOCKET_ERROR)
        return false;

    if (listen(tempListen.get(), SOMAXCONN) == SOCKET_ERROR)
        return false;

    const size_t pollBytes = sizeof(WSAPOLLFD) * (MAX_CLIENTS + 1);
    HeapPtr<WSAPOLLFD> pollFdsHolder(
        static_cast<WSAPOLLFD*>(HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, pollBytes))
    );
    if (!pollFdsHolder)
        return false;

    WSAPOLLFD* pollFds = pollFdsHolder.get();

    _listenSocket = tempListen.release();
    InterlockedExchange(&_isRunning, 1);

    pollFds[0].fd = _listenSocket;
    pollFds[0].events = POLLRDNORM;
    uint32_t pollCount = 1;

    while (InterlockedCompareExchange(&_isRunning, 1, 1) == 1)
    {
        DrainReturnedSockets(pollFds, pollCount);

        int ready = WSAPoll(pollFds, pollCount, 200);
        if (ready == SOCKET_ERROR)
        {
            if (InterlockedCompareExchange(&_isRunning, 0, 0) == 0)
                break;
            Sleep(10);
            continue;
        }

        if (ready == 0)
            continue; 

        if (pollFds[0].revents & POLLRDNORM)
        {
            UniqueSocket newClient(accept(_listenSocket, nullptr, nullptr));
            if (newClient)
            {
                uint32_t totalClients = (pollCount - 1) + static_cast<uint32_t>(InterlockedCompareExchange(&_activeWorkers, 0, 0));
                if (totalClients < MAX_CLIENTS)
                {
                    ConfigureClientSocket(newClient.get());
                    pollFds[pollCount].fd = newClient.release();
                    pollFds[pollCount].events = POLLRDNORM;
                    pollFds[pollCount].revents = 0;
                    ++pollCount;
                }
            }
        }

        uint32_t i = 1;
        while (i < pollCount)
        {
            SHORT revents = pollFds[i].revents;
            if (revents == 0)
            {
                ++i;
                continue;
            }

            SOCKET clientSock = pollFds[i].fd;

            pollFds[i] = pollFds[pollCount - 1];
            --pollCount;

            if (revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                closesocket(clientSock);
                continue;
            }

            if (revents & POLLRDNORM)
            {
                UniqueSocket sockGuard(clientSock);
                HeapPtr<ClientTask> task(static_cast<ClientTask*>(HeapAlloc(GetProcessHeap(), 0, sizeof(ClientTask))));
                if (!task)
                    continue;

                task->server = this;
                task->socket = sockGuard.get();

                TpWorkHandle work(CreateThreadpoolWork(WorkerCallback, task.get(), &_callbackEnv));
                if (!work)
                    continue;

                InterlockedIncrement(&_activeWorkers);
                task.release();
                sockGuard.release();

                SubmitThreadpoolWork(work.release());
            }
        }
    }

    for (uint32_t i = 1; i < pollCount; ++i)
    {
        closesocket(pollFds[i].fd);
    }

    EnterCriticalSection(&_returnLock);
    for (uint32_t i = 0; i < _returnedCount; ++i)
    {
        closesocket(_returnedSockets[i]);
    }
    _returnedCount = 0;
    LeaveCriticalSection(&_returnLock);

    while (InterlockedCompareExchange(&_activeWorkers, 0, 0) > 0)
    {
        Sleep(25);
    }

    return true;
}

void TcpServer::Stop() noexcept
{
    InterlockedExchange(&_isRunning, 0);
    SOCKET s = reinterpret_cast<SOCKET>(
        InterlockedExchangePointer(reinterpret_cast<PVOID*>(&_listenSocket), reinterpret_cast<PVOID>(INVALID_SOCKET))
    );
    if (s != INVALID_SOCKET)
    {
        closesocket(s);
    }
}

void TcpServer::ReturnSocketToPoll(SOCKET sock) noexcept
{
    EnterCriticalSection(&_returnLock);
    if (InterlockedCompareExchange(&_isRunning, 1, 1) == 1 && _returnedCount < MAX_CLIENTS)
    {
        _returnedSockets[_returnedCount++] = sock;
        sock = INVALID_SOCKET;
    }
    LeaveCriticalSection(&_returnLock);

    if (sock != INVALID_SOCKET)
    {
        closesocket(sock);
    }
}

void TcpServer::DrainReturnedSockets(WSAPOLLFD* pollFds, uint32_t& pollCount) noexcept
{
    EnterCriticalSection(&_returnLock);
    for (uint32_t i = 0; i < _returnedCount; ++i)
    {
        if (pollCount < MAX_CLIENTS + 1)
        {
            pollFds[pollCount].fd      = _returnedSockets[i];
            pollFds[pollCount].events  = POLLRDNORM;
            pollFds[pollCount].revents = 0;
            ++pollCount;
        }
        else
        {
            closesocket(_returnedSockets[i]);
        }
    }
    _returnedCount = 0;
    LeaveCriticalSection(&_returnLock);
}

VOID CALLBACK TcpServer::WorkerCallback(PTP_CALLBACK_INSTANCE, PVOID context, PTP_WORK work) noexcept
{
    CloseThreadpoolWork(work);

    HeapPtr<ClientTask> task(static_cast<ClientTask*>(context));
    TcpServer* server = task->server;
    UniqueSocket clientSocket(task->socket);
    task.reset();

    if (server->ProcessSingleRequest(clientSocket.get()))
    {
        server->ReturnSocketToPoll(clientSocket.release());
    }

    InterlockedDecrement(&server->_activeWorkers);
}

bool TcpServer::ProcessSingleRequest(SOCKET clientSocket) noexcept
{
    PacketHeader reqHeader;
    if (!RecvAll(clientSocket, &reqHeader, sizeof(reqHeader)))
        return false;

    if (reqHeader.magic != PROTOCOL_MAGIC)
        return false;

    switch (reqHeader.commandId)
    {
        case CommandId::GetDirList:
            return ProcessGetDirList(clientSocket, reqHeader.payloadSize);

        default:
        {
            constexpr uint32_t MAX_SKIP_PAYLOAD = 1024 * 1024;
            if (reqHeader.payloadSize > MAX_SKIP_PAYLOAD)
            {
                SendErrorResponse(clientSocket, reqHeader.commandId, StatusCode::UnknownCommand);
                return false;
            }
            
            char discardBuf[1024];
            uint32_t remaining = reqHeader.payloadSize;

            while (remaining > 0)
            {
                uint32_t chunk = (remaining < sizeof(discardBuf)) ? remaining : sizeof(discardBuf);
                if (!RecvAll(clientSocket, discardBuf, static_cast<int>(chunk)))
                    return false;
                remaining -= chunk;
            }
            
            return SendErrorResponse(clientSocket, reqHeader.commandId, StatusCode::UnknownCommand);
        }
    }
}

bool TcpServer::SendAll(SOCKET sock, const void* data, int size) const noexcept
{
    if (size < 0)
        return false;

    const char* ptr = static_cast<const char*>(data);
    int totalSent = 0;
    while (totalSent < size)
    {
        int sent = send(sock, ptr + totalSent, size - totalSent, 0);
        if (sent == SOCKET_ERROR || sent == 0)
            return false;

        totalSent += sent;
    }

    return true;
}

bool TcpServer::RecvAll(SOCKET sock, void* buffer, int size) const noexcept
{
    if (size < 0)
        return false;

    char* ptr = static_cast<char*>(buffer);
    int totalReceived = 0;
    while (totalReceived < size)
    {
        int received = recv(sock, ptr + totalReceived, size - totalReceived, 0);
        if (received == SOCKET_ERROR || received == 0)
        {
            return false;
        }
        totalReceived += received;
    }

    return true;
}

bool TcpServer::RecvUtf8Path(SOCKET sock, uint32_t byteLen, WString& outPath) const noexcept
{
    outPath.clear();
    if (byteLen == 0 || byteLen > 131072)
        return false;

    HeapPtr<char> utf8Buf(static_cast<char*>(HeapAlloc(GetProcessHeap(), 0, byteLen)));
    if (!utf8Buf)
        return false;

    if (!RecvAll(sock, utf8Buf.get(), static_cast<int>(byteLen)))
        return false;

    int charCount = MultiByteToWideChar(CP_UTF8, 0, utf8Buf.get(), static_cast<int>(byteLen), nullptr, 0);
    if (charCount <= 0)
        return false;

    wchar_t* dst = outPath.reserveForWrite(static_cast<size_t>(charCount));
    if (!dst)
        return false;

    MultiByteToWideChar(CP_UTF8, 0, utf8Buf.get(), static_cast<int>(byteLen), dst, charCount);
    outPath.commitWrite(static_cast<size_t>(charCount));
    return true;
}

bool TcpServer::SendErrorResponse(SOCKET sock, CommandId cmdId, StatusCode statusCode) const noexcept
{
    PacketHeader respHeader = { PROTOCOL_MAGIC, cmdId, statusCode, 0 };
    return SendAll(sock, &respHeader, sizeof(respHeader));
}

bool TcpServer::ProcessGetDirList(SOCKET clientSocket, uint32_t payloadSize) noexcept
{
    WString targetPath;

    // Если путь не удалось прочитать разрываем соединение (return false)
    if (payloadSize == 0 || !RecvUtf8Path(clientSocket, payloadSize, targetPath))
    {
        SendErrorResponse(clientSocket, CommandId::GetDirList, StatusCode::InvalidPath);
        return false;
    }

    ScanResult scanRes;
    ScanDirectory(targetPath, scanRes);

    if (scanRes.status != StatusCode::Ok)
    {
        return SendErrorResponse(clientSocket, CommandId::GetDirList, scanRes.status);
    }

    // Считаем общий размер ответа в байтах UTF-8
    uint32_t itemCount = scanRes.items.size();
    uint32_t totalPayloadSize = sizeof(DirListResponseHeader);

    for (uint32_t i = 0; i < itemCount; ++i)
    {
        const WString& name = scanRes.items[i].name;
        int utf8Bytes = 0;
        if (!name.empty())
        {
            utf8Bytes = WideCharToMultiByte(
                CP_UTF8, 0,
                name.c_str(), static_cast<int>(name.size()),
                nullptr, 0, nullptr, nullptr
            );
        }
        if (utf8Bytes < 0) utf8Bytes = 0;
        totalPayloadSize += sizeof(FileItemHeader) + static_cast<uint32_t>(utf8Bytes);
    }

    PacketHeader respHeader = { PROTOCOL_MAGIC, CommandId::GetDirList, StatusCode::Ok, totalPayloadSize };
    if (!SendAll(clientSocket, &respHeader, sizeof(respHeader)))
        return false;

    DirListResponseHeader listHeader = { itemCount };
    if (!SendAll(clientSocket, &listHeader, sizeof(listHeader)))
        return false;

    constexpr int MAX_UTF8_NAME_BYTES = MAX_PATH * 4;
    char utf8NameBuf[MAX_UTF8_NAME_BYTES];

    for (uint32_t i = 0; i < itemCount; ++i)
    {
        const ScannedItem& item = scanRes.items[i];
        uint16_t utf8Len = 0;

        if (!item.name.empty())
        {
            int written = WideCharToMultiByte(
                CP_UTF8, 0,
                item.name.c_str(), static_cast<int>(item.name.size()),
                utf8NameBuf, sizeof(utf8NameBuf),
                nullptr, nullptr
            );
            if (written > 0)
            {
                utf8Len = static_cast<uint16_t>(written);
            }
        }

        FileItemHeader itemHdr;
        itemHdr.isDirectory = item.isDirectory ? 1 : 0;
        itemHdr.size        = item.size;
        itemHdr.nameLength  = utf8Len;

        if (!SendAll(clientSocket, &itemHdr, sizeof(itemHdr)))
            return false;

        if (utf8Len > 0 && !SendAll(clientSocket, utf8NameBuf, utf8Len))
            return false;
    }

    return true;
}