#include "Service.h"
#include "TcpServer.h"
#include "lib/WString.h"
#include "lib/UniqueHandle.h"

namespace {

constexpr wchar_t SERVICE_NAME[] = L"FileExplorerTcpServer";

SERVICE_STATUS        g_serviceStatus = {};
SERVICE_STATUS_HANDLE g_statusHandle  = nullptr;
TcpServer*            g_serverPtr     = nullptr;
uint16_t              g_servicePort   = 8080;

struct ScHandleDeleter {
    static void Close(SC_HANDLE h) noexcept {
        CloseServiceHandle(h);
    }
};
using ScHandle = UniqueHandle<SC_HANDLE, ScHandleDeleter>;

void ReportServiceStatus(DWORD currentState, DWORD win32ExitCode, DWORD waitHint) noexcept
{
    static DWORD checkPoint = 1;

    g_serviceStatus.dwServiceType             = SERVICE_WIN32_OWN_PROCESS;
    g_serviceStatus.dwCurrentState            = currentState;
    g_serviceStatus.dwWin32ExitCode           = win32ExitCode;
    g_serviceStatus.dwServiceSpecificExitCode = 0;
    g_serviceStatus.dwWaitHint                = waitHint;

    if (currentState == SERVICE_START_PENDING)
        g_serviceStatus.dwControlsAccepted = 0;
    else
        g_serviceStatus.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;

    if (currentState == SERVICE_RUNNING || currentState == SERVICE_STOPPED)
        g_serviceStatus.dwCheckPoint = 0;
    else
        g_serviceStatus.dwCheckPoint = checkPoint++;

    SetServiceStatus(g_statusHandle, &g_serviceStatus);
}

// Обработчик команд от диспетчера служб (Остановка / Выключение ПК)
void WINAPI ServiceCtrlHandler(DWORD ctrlCode) noexcept
{
    switch (ctrlCode)
    {
        case SERVICE_CONTROL_STOP:
        case SERVICE_CONTROL_SHUTDOWN:
            ReportServiceStatus(SERVICE_STOP_PENDING, NO_ERROR, 3000);
            if (g_serverPtr)
            {
                g_serverPtr->Stop();
            }
            break;
        default:
            break;
    }
}

void WINAPI ServiceMain(DWORD, LPWSTR*) noexcept
{
    g_statusHandle = RegisterServiceCtrlHandlerW(SERVICE_NAME, ServiceCtrlHandler);
    if (!g_statusHandle)
        return;

    ReportServiceStatus(SERVICE_START_PENDING, NO_ERROR, 3000);

    TcpServer server;
    g_serverPtr = &server;

    ReportServiceStatus(SERVICE_RUNNING, NO_ERROR, 0);

    bool started = server.Start(g_servicePort);

    g_serverPtr = nullptr;
    ReportServiceStatus(SERVICE_STOPPED, started ? NO_ERROR : ERROR_SERVICE_SPECIFIC_ERROR, 0);
}

} // namespace

bool RunAsService(uint16_t port) noexcept
{
    g_servicePort = port;

    SERVICE_TABLE_ENTRYW serviceTable[] =
    {
        { const_cast<LPWSTR>(SERVICE_NAME), ServiceMain },
        { nullptr, nullptr }
    };

    return StartServiceCtrlDispatcherW(serviceTable) != FALSE;
}

bool InstallWindowsService(const wchar_t* serviceName, const wchar_t* displayName, uint16_t port) noexcept
{
    WString exePath;
    wchar_t* buf = exePath.reserveForWrite(MAX_PATH);
    if (!buf) return false;

    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH) return false;
    exePath.commitWrite(len);

    WString binPath;
    binPath += L"\"";
    binPath += exePath;
    binPath += L"\" --service";
    if (port != 8080)
    {
        binPath += L" --port ";
        binPath.appendUInt(port);
    }

    ScHandle hSCManager(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE));
    if (!hSCManager) return false;

    ScHandle hService(CreateServiceW(
        hSCManager.get(),
        serviceName,
        displayName,
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        binPath.c_str(),
        nullptr, nullptr, nullptr, nullptr, nullptr
    ));

    if (!hService) return false;

    return StartServiceW(hService.get(), 0, nullptr) != FALSE;
}

bool RemoveWindowsService(const wchar_t* serviceName) noexcept
{
    auto hSCManager = ScHandle(OpenSCManagerW(nullptr, nullptr, SC_MANAGER_CONNECT));
    if (!hSCManager)
        return false;

    auto hService = ScHandle(OpenServiceW(hSCManager.get(), serviceName, DELETE | SERVICE_STOP));
    if (!hService)
        return false;

    SERVICE_STATUS status;
    ControlService(hService.get(), SERVICE_CONTROL_STOP, &status);

    return DeleteService(hService.get()) != FALSE;
}