cmake_minimum_required(VERSION 3.16)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS 
    "*.cpp" 
    "*.h"
)

add_executable(FileExplorerServer ${SOURCES})

target_include_directories(FileExplorerServer PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../common")

# Подключаем системные библиотеки Windows:
# ws2_32 - Winsock2 (TCP-сокеты)
# advapi32 - управление службами Windows (SCM)
# shell32 - CommandLineToArgvW
# Включаем Unicode (чтобы все функции WinAPI по умолчанию были с суффиксом W)
target_link_libraries(FileExplorerServer PRIVATE kernel32 user32 ws2_32 advapi32 shell32)
target_compile_definitions(FileExplorerServer PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN)

if(MSVC)
    string(REGEX REPLACE "/RTC(su|[1su])" "" CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG}")
    target_compile_options(FileExplorerServer PRIVATE /W4 /EHs-c- /GR- /GS- /Zc:threadSafeInit-)
    target_link_options(FileExplorerServer PRIVATE /NODEFAULTLIB /ENTRY:wmainCRTStartup /SUBSYSTEM:CONSOLE)
elseif(MINGW)
    target_compile_options(FileExplorerServer PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -fno-threadsafe-statics)
    target_link_options(FileExplorerServer PRIVATE
        -nostdlib
        -mconsole
        -Wl,-e,wmainCRTStartup
    )
elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
    target_compile_options(FileExplorerServer PRIVATE -Wall -Wextra -fno-exceptions -fno-rtti -fno-stack-protector -fno-threadsafe-statics)
    target_link_options(FileExplorerServer PRIVATE
        -Wl,/NODEFAULTLIB
        -Wl,/ENTRY:wmainCRTStartup
        -Wl,/SUBSYSTEM:CONSOLE
    )
endif()