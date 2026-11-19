#pragma once
#include <cstdint>

//   Модуль интеграции с диспетчером служб Windows (SCM).
 
//   Основные функции:
//   - RunAsService(): регистрирует обработчик команд остановки (ServiceCtrlHandler) и запускает
//     экземпляр TcpServer в контексте фоновой службы Windows.
//   - InstallWindowsService() / RemoveWindowsService(): регистрируют и удаляют службу в системе
//     с автоматическим освобождением дескрипторов SCM через RAII.


// Запуск в режиме службы Windows (--service)
bool RunAsService(uint16_t port) noexcept;

// Установки и удаления службы (--install / --remove)
bool InstallWindowsService(const wchar_t* serviceName, const wchar_t* displayName, uint16_t port = 8080) noexcept;
bool RemoveWindowsService(const wchar_t* serviceName) noexcept;