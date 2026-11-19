#pragma once

#include <windows.h>
#include <stdint.h>

class WString
{
public:
    static size_t rawLength(const wchar_t* str) noexcept;
public:
    WString() noexcept;
    WString(const wchar_t* str) noexcept;
    WString(const WString& other) noexcept;
    WString(WString&& other) noexcept;
    WString& operator=(const WString& other) noexcept;
    WString& operator=(WString&& other) noexcept;
    WString& operator=(const wchar_t* str) noexcept;
    ~WString();

    bool operator==(const WString& other) const noexcept;
    bool operator!=(const WString& other) const noexcept;
    bool operator==(const wchar_t* str) const noexcept;
    bool operator!=(const wchar_t* str) const noexcept;

    WString& operator+=(const wchar_t* str) noexcept;
    WString& operator+=(const WString& other) noexcept;

    wchar_t& operator[](size_t index) noexcept;
    const wchar_t& operator[](size_t index) const noexcept;

    WString& append(const wchar_t* str, size_t strLen) noexcept;
    WString& append(const wchar_t* str) noexcept;
    WString& append(const WString& other) noexcept;

    WString& assign(const wchar_t* str) noexcept;

    void resize(size_t newLen) noexcept;
    bool equals(const wchar_t* str, size_t strLen) const noexcept;

    wchar_t back() const noexcept;
    void push_back(wchar_t ch) noexcept;

    bool startsWith(const wchar_t* prefix) const noexcept;

    void clear() noexcept;
    void reset() noexcept;
    size_t capacity() const noexcept { return _cap; }
    const wchar_t* c_str() const noexcept { return activeBuf(); }
    size_t size() const noexcept { return _len; }
    bool empty() const noexcept { return _len == 0; }

    WString& appendUInt(uint64_t val) noexcept;
    WString& appendPadded(const WString& str, size_t width) noexcept;
    wchar_t* reserveForWrite(size_t exactChars) noexcept;
    void commitWrite(size_t writtenChars) noexcept;
protected:
    wchar_t* activeBuf() noexcept { return _heapPtr ? _heapPtr : _buff; }
    const wchar_t* activeBuf() const noexcept { return _heapPtr ? _heapPtr : _buff; }
    bool ensureCapacity(size_t required) noexcept;
private:
    static constexpr size_t SSO_SIZE = MAX_PATH;

    wchar_t _buff[SSO_SIZE];
    wchar_t* _heapPtr;
    size_t _len;
    size_t _cap;
};

inline bool operator==(const wchar_t* lhs, const WString& rhs) noexcept { return rhs == lhs; }
inline bool operator!=(const wchar_t* lhs, const WString& rhs) noexcept { return rhs != lhs; }