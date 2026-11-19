find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Widgets Network)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Widgets Network)

set(CMAKE_AUTOMOC ON)

file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS 
    "*.cpp" 
    "*.h"
)

add_executable(FileExplorerClient WIN32 ${SOURCES})

target_include_directories(FileExplorerClient PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/../common")

if(WIN32)
    get_target_property(_qmake_executable Qt${QT_VERSION_MAJOR}::qmake IMPORTED_LOCATION)
    get_filename_component(_qt_bin_dir "${_qmake_executable}" DIRECTORY)
    find_program(WINDEPLOYQT_EXECUTABLE windeployqt HINTS "${_qt_bin_dir}")

    if(WINDEPLOYQT_EXECUTABLE)
        add_custom_command(TARGET FileExplorerClient POST_BUILD
            COMMAND "${WINDEPLOYQT_EXECUTABLE}"
                --no-translations
                --no-system-d3d-compiler
                --no-opengl-sw
                "$<TARGET_FILE:FileExplorerClient>"
            COMMENT "Deploying Qt dependencies for FileExplorerClient..."
        )
    endif()
endif()

# Подключаем модули Qt Widgets (интерфейс) и Qt Network (QTcpSocket)
target_link_libraries(FileExplorerClient PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Network
)