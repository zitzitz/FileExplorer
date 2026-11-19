#pragma once

#include "lib/WString.h"
#include "lib/HeapArray.h"
#include "Protocol.h"


struct ScannedItem
{
    bool isDirectory = false;
    uint64_t size = 0;
    WString name;
};

struct ScanResult
{
    StatusCode status = StatusCode::Ok;
    HeapArray<ScannedItem> items;

    void clear() noexcept
    {
        items.clear();
        status = StatusCode::Ok;
    }
};

void ScanDirectory(const WString& dirPath, ScanResult& outResult) noexcept;