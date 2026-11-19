#pragma once

#include <stdint.h>

#pragma pack(push, 1)

constexpr uint32_t PROTOCOL_MAGIC = 0x56525346;

enum class CommandId : uint16_t {
    GetDirList = 1
};

enum class StatusCode : uint16_t {
    Ok             = 0,
    PathNotFound   = 1,
    AccessDenied   = 2,
    InvalidPath    = 3,
    UnknownCommand = 4,
    InternalError  = 5
};

// Заголовок пакета
struct PacketHeader {
    uint32_t magic;
    CommandId commandId;
    StatusCode status;
    uint32_t payloadSize;
};

// колличество файлов
struct DirListResponseHeader {
    uint32_t itemsCount;
};

// Описание одного файла или папки в ответе, cразу за этой структурой в потоке идут (nameLength * sizeof(wchar_t)) байт
struct FileItemHeader {
    uint8_t  isDirectory; // 1 = папка, 0 = файл
    uint64_t size;        // Размер файла или суммарный размер папки в байтах
    uint16_t nameLength;  // Длина имени в символах (wchar_t), не считая '\0'
};

#pragma pack(pop)

