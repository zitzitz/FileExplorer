#include <windows.h>
#include <shellapi.h>
#include "TcpServer.h"
#include "lib/WString.h"
#include "Service.h"

// Заглушки рантайма для сборки без стандартной библиотеки
void* __cdecl operator new(size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void* __cdecl operator new[](size_t size) {
    return HeapAlloc(GetProcessHeap(), 0, size);
}

void __cdecl operator delete(void* ptr) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void __cdecl operator delete(void* ptr, size_t) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void __cdecl operator delete[](void* ptr) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

void __cdecl operator delete[](void* ptr, size_t) noexcept {
    if (ptr) {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
}

extern "C" void* __cdecl memset(void* dest, int val, size_t count) {
    unsigned char* ptr = static_cast<unsigned char*>(dest);
    while (count--) {
        *ptr++ = static_cast<unsigned char>(val);
    }
    return dest;
}

extern "C" void* __cdecl memcpy(void* dest, const void* src, size_t count) {
    unsigned char* d = static_cast<unsigned char*>(dest);
    const unsigned char* s = static_cast<const unsigned char*>(src);
    while (count--) {
        *d++ = *s++;
    }
    return dest;
}

// Вывод текста в консоль через WinAPI
void PrintConsole(const WString& text) noexcept
{
    if (text.empty())
		return;

    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut != INVALID_HANDLE_VALUE && hOut != nullptr)
    {
        DWORD written = 0;
        WriteConsoleW(hOut, text.c_str(), static_cast<DWORD>(text.size()), &written, nullptr);
    }
}

// Парсинг строки с номером порта (1..65535)
bool ParsePort(const wchar_t* str, uint16_t& outPort) noexcept
{
    if (!str || str[0] == L'\0')
        return false;

    uint32_t val = 0;
    for (size_t i = 0; str[i] != L'\0'; ++i)
    {
        if (str[i] < L'0' || str[i] > L'9')
            return false;

        val = val * 10 + static_cast<uint32_t>(str[i] - L'0');
        if (val > 65535)
            return false;
    }

    if (val == 0)
        return false;

    outPort = static_cast<uint16_t>(val);
    return true;
}

// Проверка что терминал запущен с правами администратора
bool IsRunAsAdmin() noexcept
{
    BOOL isAdmin = FALSE;
    SID_IDENTIFIER_AUTHORITY ntAuthority = SECURITY_NT_AUTHORITY;
    PSID adminGroup = nullptr;

    if (AllocateAndInitializeSid(
            &ntAuthority, 2,
            SECURITY_BUILTIN_DOMAIN_RID,
            DOMAIN_ALIAS_RID_ADMINS,
            0, 0, 0, 0, 0, 0,
            &adminGroup))
    {
        CheckTokenMembership(nullptr, adminGroup, &isAdmin);
        FreeSid(adminGroup);
    }

    return isAdmin != FALSE;
}

// перезапускам терминал с парвами администратора если они отсутствуют
bool RelaunchAsAdmin(const wchar_t* args) noexcept
{
    WString exePath;
    wchar_t* buf = exePath.reserveForWrite(MAX_PATH);
    if (!buf)
		return false;

    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0 || len >= MAX_PATH)
		return false;
    exePath.commitWrite(len);

    HINSTANCE res = ShellExecuteW(
        nullptr,
        L"runas",
        exePath.c_str(),
        args,
        nullptr,
        SW_SHOWNORMAL
    );

    return reinterpret_cast<INT_PTR>(res) > 32;
}

int RunAsConsole(uint16_t port) {
    TcpServer server;

	WString msg(L"TCP Server listening on port ");
    msg.appendUInt(port);
    msg += L"...\r\n";
    PrintConsole(msg.c_str());

    if (!server.Start(port)) {
        PrintConsole(L"Failed to start server!\r\n");
        return 1;
    }
    return 0;
}

int Main(int argc, wchar_t* argv[]) {
	
	uint16_t port = 8080;
    WString mode;

	for (int i = 1; i < argc; ++i)
    {
        WString arg(argv[i]);

        if (arg == L"--service" || arg == L"--install" || arg == L"--uninstall" || arg == L"--console")
        {
            mode = arg;
        }
        else if (arg == L"--port")
        {
            if (i + 1 >= argc || !ParsePort(argv[++i], port))
            {
                PrintConsole(L"Ошибка: укажите корректный порт (1-65535) после --port.\r\n");
                return 1;
            }
        }
        else if (arg.startsWith(L"--port="))
        {
            if (!ParsePort(arg.c_str() + 7, port))
            {
                PrintConsole(L"Ошибка: некорректный порт в параметре --port=...\r\n");
                return 1;
            }
        }
        else if (!ParsePort(arg.c_str(), port))
        {
            PrintConsole(L"Неизвестный аргумент. Использование: [--console | --service | --install | --uninstall] [--port <1-65535>]\r\n");
            return 1;
        }
    }

	if (mode == L"--service")
    {
        return RunAsService(port) ? 0 : 1;
    }
    else if (mode == L"--install" || mode == L"--uninstall")
    {
        if (!IsRunAsAdmin())
        {
            WString args = mode;
            if (port != 8080)
            {
                args += L" --port ";
                args.appendUInt(port);
            }

            if (RelaunchAsAdmin(args.c_str()))
            {
                return 0;
            }
            PrintConsole(L"Не удалось получить права администратора.\r\n");
            return 1;
        }

        if (mode == L"--install")
        {
            return InstallWindowsService(L"FileExplorerTcpServer", L"Remote File Explorer Server", port) ? 0 : 1;
        }
        else
        {
            return RemoveWindowsService(L"FileExplorerTcpServer") ? 0 : 1;
        }
    }

    return RunAsConsole(port);
}

// Точка входа приложения без инициализации стандартной библиотеки C
extern "C" void wmainCRTStartup() {
	int argc = 0;
	wchar_t** argv = CommandLineToArgvW(GetCommandLineW(), &argc);

	int exit = 1;

	if (argv != nullptr) {
		exit = Main(argc, argv);

		LocalFree(argv);
	}
    
    ExitProcess(exit);
}