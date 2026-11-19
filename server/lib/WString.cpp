#include "WString.h"

size_t WString::rawLength(const wchar_t* str) noexcept {
    if (!str)
        return 0;

    size_t len = 0;
    while (str[len] != L'\0')
        ++len;

    return len;
}

WString::WString() noexcept
:_heapPtr(nullptr)
,_len(0)
,_cap(SSO_SIZE - 1)
{
    _buff[0] = L'\0';
}

WString::WString(const wchar_t* str) noexcept
:WString()
{
    append(str);
}

WString::WString(const WString& other) noexcept
: WString()
{
    append(other);
}

WString::WString(WString&& other) noexcept
:_heapPtr(other._heapPtr)
,_len(other._len)
,_cap(other._cap)
{
    if (!_heapPtr)
    {
        CopyMemory(_buff, other._buff, (_len + 1) * sizeof(wchar_t));
    }
    other._heapPtr = nullptr;
    other._len = 0;
    other._cap = SSO_SIZE - 1;
    other._buff[0] = L'\0';
}

WString::~WString()
{
    if (_heapPtr)
    {
        HeapFree(GetProcessHeap(), 0, _heapPtr);
        _heapPtr = nullptr;
    }
    _len = 0;
    _cap = SSO_SIZE - 1;
    _buff[0] = L'\0';
}

WString& WString::operator=(const WString& other) noexcept {
    if (this != &other) {
        clear();
        append(other);
    }

    return *this;
}

WString& WString::operator=(WString&& other) noexcept {
    if (this != &other) {
        if (_heapPtr)
            HeapFree(GetProcessHeap(), 0, _heapPtr);
        _heapPtr = other._heapPtr;
        _len = other._len;
        _cap = other._cap;
        if (!_heapPtr) {
            CopyMemory(_buff, other._buff, (_len + 1) * sizeof(wchar_t));
        }
        other._heapPtr = nullptr;
        other._len = 0;
        other._cap = SSO_SIZE - 1;
        other._buff[0] = L'\0';
    }
    return *this;
}

WString& WString::operator=(const wchar_t* str) noexcept
{ 
    return assign(str); 
}

bool WString::operator==(const WString& other) const noexcept
{ 
    return equals(other.c_str(), other._len); 
}

bool WString::operator!=(const WString& other) const noexcept
{ 
    return !(*this == other); 
}

bool WString::operator==(const wchar_t* str) const noexcept
{ 
    return equals(str, rawLength(str)); 
}
bool WString::operator!=(const wchar_t* str) const noexcept
{ 
    return !(*this == str); 
}

WString& WString::operator+=(const wchar_t* str) noexcept
{
    return append(str);
}

WString& WString::operator+=(const WString& other) noexcept
{
    return append(other);
}

wchar_t& WString::operator[](size_t index) noexcept
{
    return activeBuf()[index];
}
const wchar_t& WString::operator[](size_t index) const noexcept
{
    return activeBuf()[index];
}

WString& WString::append(const wchar_t* str, size_t strLen) noexcept
{
    if (!str || strLen == 0)
        return *this;
    if (ensureCapacity(_len + strLen)) {
        wchar_t* buff = activeBuf();
        CopyMemory(buff + _len, str, strLen * sizeof(wchar_t));
        _len += strLen;
        buff[_len] = L'\0';
    }
    return *this;
}

WString& WString::append(const wchar_t* str) noexcept
{
    return append(str, rawLength(str));
}

WString& WString::append(const WString& other) noexcept
{
    return append(other.c_str(), other._len);
}

WString& WString::assign(const wchar_t* str) noexcept
{
    clear();
    return append(str);
}

void WString::resize(size_t newLen) noexcept
{
    if (newLen < _len)
    {
        _len = newLen;
        activeBuf()[_len] = L'\0';
    }
}

bool WString::equals(const wchar_t* str, size_t strLen) const noexcept
{
    if (_len != strLen)
        return false;

    const wchar_t* buff = activeBuf();
    for (size_t i = 0; i < _len; ++i)
    {
        if (buff[i] != str[i])
            return false;
    }

    return true;
}

wchar_t WString::back() const noexcept
{
    return _len > 0 ? activeBuf()[_len - 1] : L'\0';
}

void WString::push_back(wchar_t ch) noexcept
{
    if (ensureCapacity(_len + 1)) {
        wchar_t* buff = activeBuf();
        buff[_len++] = ch;
        buff[_len] = L'\0';
    }
}

bool WString::startsWith(const wchar_t* prefix) const noexcept
{
    size_t prefixLen = rawLength(prefix);
    if (_len < prefixLen)
        return false;

    const wchar_t* buff = activeBuf();
    for (size_t i = 0; i < prefixLen; ++i)
    {
        if (buff[i] != prefix[i])
            return false;
    }
    return true;
}

void WString::clear() noexcept {
    _len = 0;
    activeBuf()[0] = L'\0';
}

void WString::reset() noexcept
{
    if (_heapPtr)
    {
        HeapFree(GetProcessHeap(), 0, _heapPtr);
        _heapPtr = nullptr;
    }
    _len = 0;
    _cap = SSO_SIZE - 1;
    _buff[0] = L'\0';
}

bool WString::ensureCapacity(size_t required) noexcept
{
    if (_cap == 0)
        _cap = SSO_SIZE - 1;

    if (required <= _cap)
        return true;

    size_t newCap = (_cap > SIZE_MAX / 2) ? required : (_cap * 2);
    if (newCap < required)
        newCap = required;

    HANDLE heap = GetProcessHeap();
    size_t bytes = (newCap + 1) * sizeof(wchar_t);

    if (!_heapPtr)
    {
        wchar_t* newBuf = static_cast<wchar_t*>(HeapAlloc(heap, 0, bytes));
        if (!newBuf)
            return false;
        CopyMemory(newBuf, _buff, (_len + 1) * sizeof(wchar_t));
        _heapPtr = newBuf;
    }
    else
    {
        wchar_t* newBuf = static_cast<wchar_t*>(HeapReAlloc(heap, 0, _heapPtr, bytes));
        if (!newBuf)
            return false;
        _heapPtr = newBuf;
    }

    _cap = newCap;
    return true;
}

WString& WString::appendUInt(uint64_t val) noexcept
{
    wchar_t tmp[24]; //uint64_t max 20
    size_t i = 0;
    do
    {
        tmp[i++] = static_cast<wchar_t>(L'0' + (val % 10));
        val /= 10;
    } while (val > 0);

    if (ensureCapacity(_len + i))
    {
        wchar_t* buff = activeBuf();
        while (i > 0)
        {
            buff[_len++] = tmp[--i];
        }
        buff[_len] = L'\0';
    }
    return *this;
}

WString& WString::appendPadded(const WString& str, size_t width) noexcept
{
    append(str);
    size_t pad = (str.size() < width) ? (width - str.size()) : 0;
    if (pad > 0 && ensureCapacity(_len + pad))
    {
        wchar_t* buff = activeBuf();
        for (size_t i = 0; i < pad; ++i)
        {
            buff[_len++] = L' ';
        }
        buff[_len] = L'\0';
    }
    return *this;
}

wchar_t* WString::reserveForWrite(size_t exactChars) noexcept
{
    if (!ensureCapacity(exactChars))
        return nullptr;

    return activeBuf();
}

void WString::commitWrite(size_t writtenChars) noexcept
{
    if (writtenChars <= _cap)
    {
        _len = writtenChars;
        activeBuf()[_len] = L'\0';
    }
}