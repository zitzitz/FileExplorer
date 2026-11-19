# Клиент-серверный сканер каталогов (Тестовое задание)

Проект состоит из двух приложений, объединённых общим `CMakeLists.txt`:
1. **FileExplorerServer** — многопоточный TCP-сервер на чистом WinAPI (без использования CRT/STL) с диспетчеризацией через `WSAPoll` и системный пул потоков Windows. Поддерживает работу в консольном режиме и в качестве фоновой службы Windows.
2. **FileExplorerClient** — графический клиент на Qt (архитектура Model/View + асинхронный сетевой слой).

## Требования для сборки
* CMake 3.16 или выше
* Компилятор: MinGW (GCC), MSVC (Visual Studio 2019/2022/2026) или Clang
* Qt 5.15+ или Qt 6.x (модули `Widgets` и `Network`)

## Инструкция по сборке

### Вариант 1: Через Qt Creator (MinGW или MSVC)
1. Откройте корневой файл `CMakeLists.txt` в Qt Creator.
2. Выберите нужный Kit с установленным Qt.
3. Соберите проект (`Ctrl + Shift + B`). В меню выбора цели запуска доступны `FileExplorerServer` и `FileExplorerClient`.

### Вариант 2: Генерация решения для Visual Studio
Выполните в консоли из корня проекта, указав путь к вашей установке Qt под MSVC:
```cmd
cmake -B build -G "Visual Studio 18 2026" -DCMAKE_PREFIX_PATH="C:\Qt\6.8.0\msvc2019_64"
```
После этого откройте сгенерированный файл `build\FileExplorer.sln` в Visual Studio (или выполните команду `cmake --build build --config Release`).

## Запуск

* **Сервер (консольный режим):**
  ```cmd
  FileExplorerServer.exe
  FileExplorerServer.exe --console --port 9090
  ```

  По умолчанию сервер слушает порт **8080**

* **Сервер (служба Windows)** *(при запуске из обычного терминала автоматически запросит права администратора через окно UAC)*:
  * Установка и запуск: `FileExplorerServer.exe --install` (или `FileExplorerServer.exe --install --port 9090`)
  * Остановка и удаление: `FileExplorerServer.exe --uninstall`
* **Клиент:**
  * Запустите `FileExplorerClient.exe` (по умолчанию настроен на `127.0.0.1:8080`).