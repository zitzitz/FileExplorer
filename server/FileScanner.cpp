#include "FileScanner.h"
#include "lib/UniqueHandle.h"

namespace {

void AppendPathSegment(WString& path, const wchar_t* segment) noexcept;

struct FindHandleDeleter {
    static HANDLE Invalid() noexcept { 
        return INVALID_HANDLE_VALUE; 
    }
    
    static void Close(HANDLE h) noexcept { 
        FindClose(h); 
    }
};

struct ProcessHeapDeleter {
    static void Close(void* ptr) noexcept {
        HeapFree(GetProcessHeap(), 0, ptr);
    }
};

using FindHandle = UniqueHandle<HANDLE, FindHandleDeleter>;
template <typename T>
using HeapPtr = UniqueHandle<T*, ProcessHeapDeleter>;

class ScopedPathAppend
{
    WString& _path;
    size_t   _baseLen;

public:
    ScopedPathAppend(WString& path, const wchar_t* segment) noexcept
        : _path(path), _baseLen(path.size())
    {
        AppendPathSegment(_path, segment);
    }

    ~ScopedPathAppend() noexcept
    {
        _path.resize(_baseLen);
    }

    ScopedPathAppend(const ScopedPathAppend&) = delete;
    ScopedPathAppend& operator=(const ScopedPathAppend&) = delete;
};

bool IsDotDirectory(const wchar_t* name) noexcept
{
    if (name[0] == L'.')
    {
        if (name[1] == L'\0') return true;
        if (name[1] == L'.' && name[2] == L'\0') return true;
    }
    return false;
}

void AppendPathSegment(WString& path, const wchar_t* segment) noexcept
{
    if (!segment || segment[0] == L'\0') return;
    if (!path.empty() && path.back() != L'\\' && path.back() != L'/')
    {
        path.push_back(L'\\');
    }
    while (*segment == L'\\' || *segment == L'/')
    {
        ++segment;
    }
    path += segment;
}

WString MakeExtendedPath(const WString& rawPath) noexcept
{
    WString result;
    if (rawPath.empty()) return result;

    DWORD needed = GetFullPathNameW(rawPath.c_str(), 0, nullptr, nullptr);
    if (needed == 0) return result;

    HeapPtr<wchar_t> fullBuf(static_cast<wchar_t*>(HeapAlloc(GetProcessHeap(), 0, needed * sizeof(wchar_t))));
    if (!fullBuf) return result;

    DWORD len = GetFullPathNameW(rawPath.c_str(), needed, fullBuf.get(), nullptr);
    if (len == 0 || len >= needed)
        return result;

    const wchar_t* path = fullBuf.get();
    if (len >= 4 && path[0] == L'\\' && path[1] == L'\\' && path[2] == L'?' && path[3] == L'\\')
    {
        result = path;
    }
    else if (len >= 2 && path[0] == L'\\' && path[1] == L'\\')
    {
        result = L"\\\\?\\UNC\\";
        result += (path + 2);
    }
    else
    {
        result = L"\\\\?\\";
        result += path;
    }

    return result;
}

StatusCode MapWinErrorToStatus(DWORD err) noexcept
{
    switch (err)
    {
        case ERROR_FILE_NOT_FOUND:
        case ERROR_PATH_NOT_FOUND:
        case ERROR_INVALID_NAME:
        case ERROR_BAD_NETPATH:
            return StatusCode::PathNotFound;
        case ERROR_ACCESS_DENIED:
            return StatusCode::AccessDenied;
        default:
            return StatusCode::InternalError;
    }
}

uint64_t CalculateFolderSize(WString& currentPath) noexcept
{
    WIN32_FIND_DATAW findData;
    FindHandle hFind;
    {
        ScopedPathAppend mask(currentPath, L"*");
        hFind.reset(FindFirstFileW(currentPath.c_str(), &findData));
    }

    if (!hFind)
        return 0;

    uint64_t totalSize = 0;
    do
    {
        if (IsDotDirectory(findData.cFileName))
            continue;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            if (!(findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT))
            {
                ScopedPathAppend subDir(currentPath, findData.cFileName);
                totalSize += CalculateFolderSize(currentPath);
            }
        }
        else
        {
            uint64_t fileSize = (static_cast<uint64_t>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
            totalSize += fileSize;
        }
    } while (FindNextFileW(hFind.get(), &findData));

    return totalSize;
}

} // namespace

void ScanDirectory(const WString& dirPath, ScanResult& outResult) noexcept
{
    outResult.clear();

    if (dirPath.empty())
    {
        outResult.status = StatusCode::InvalidPath;
        return;
    }

    WString workPath = MakeExtendedPath(dirPath);
    if (workPath.empty())
    {
        outResult.status = MapWinErrorToStatus(GetLastError());
        return;
    }

    DWORD attrs = GetFileAttributesW(workPath.c_str());
    if (attrs == INVALID_FILE_ATTRIBUTES)
    {
        outResult.status = MapWinErrorToStatus(GetLastError());
        return;
    }
    if (!(attrs & FILE_ATTRIBUTE_DIRECTORY))
    {
        outResult.status = StatusCode::InvalidPath;
        return;
    }

    WIN32_FIND_DATAW findData;
    FindHandle hFind;
    {
        ScopedPathAppend mask(workPath, L"*");
        hFind.reset(FindFirstFileW(workPath.c_str(), &findData));
    }

    if (!hFind)
    {
        outResult.status = MapWinErrorToStatus(GetLastError());
        return;
    }

    do
    {
        if (IsDotDirectory(findData.cFileName))
        {
            continue;
        }

        ScannedItem* item = outResult.items.appendEmpty();
        if (!item)
        {
            outResult.clear();
            outResult.status = StatusCode::InternalError;
            return;
        }

        item->name = findData.cFileName;

        if (findData.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)
        {
            item->isDirectory = true;
            if (findData.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT)
            {
                item->size = 0;
            }
            else
            {
                ScopedPathAppend subDir(workPath, findData.cFileName);
                item->size = CalculateFolderSize(workPath);
            }
        }
        else
        {
            item->isDirectory = false;
            item->size = (static_cast<uint64_t>(findData.nFileSizeHigh) << 32) | findData.nFileSizeLow;
        }

    } while (FindNextFileW(hFind.get(), &findData));

    outResult.status = StatusCode::Ok;
}