#pragma once

#include <windows.h>
#include <stdint.h>

inline void* operator new(size_t, void* ptr) noexcept { return ptr; }
inline void operator delete(void*, void*) noexcept {}

template <typename T>
class HeapArray
{
public:
    HeapArray() noexcept = default;

    ~HeapArray() noexcept
    {
        clear();
        freeRaw(_data);
    }

    HeapArray(const HeapArray&) = delete;
    HeapArray& operator=(const HeapArray&) = delete;

    // Поддержка перемещения
    HeapArray(HeapArray&& other) noexcept
        : _data(other._data), _count(other._count), _capacity(other._capacity)
    {
        other._data = nullptr;
        other._count = 0;
        other._capacity = 0;
    }

    HeapArray& operator=(HeapArray&& other) noexcept
    {
        if (this != &other)
        {
            clear();
            freeRaw(_data);
            _data = other._data;
            _count = other._count;
            _capacity = other._capacity;
            other._data = nullptr;
            other._count = 0;
            other._capacity = 0;
        }
        return *this;
    }

    T* appendEmpty() noexcept
    {
        if (_count >= _capacity)
        {
            if (!grow())
                return nullptr;
        }

        T* slot = &_data[_count++];
        new (slot) T();
        return slot;
    }

    void clear() noexcept
    {
        if (_data)
        {
            for (uint32_t i = 0; i < _count; ++i)
            {
                _data[i].~T();
            }
        }
        _count = 0;
    }

    T& operator[](uint32_t index) noexcept { return _data[index]; }
    const T& operator[](uint32_t index) const noexcept { return _data[index]; }

    uint32_t size() const noexcept { return _count; }
    bool empty() const noexcept { return _count == 0; }
protected:
    static T* allocRaw(uint32_t cap) noexcept
    {
        return static_cast<T*>(HeapAlloc(GetProcessHeap(), 0, static_cast<size_t>(cap) * sizeof(T)));
    }

    static void freeRaw(T* ptr) noexcept
    {
        if (ptr)
            HeapFree(GetProcessHeap(), 0, ptr);
    }

    bool grow() noexcept
    {
        uint32_t newCap = (_capacity == 0) ? 32 : (_capacity * 2);
        T* newData = allocRaw(newCap);
        if (!newData)
            return false;

        for (uint32_t i = 0; i < _count; ++i)
        {
            new (&newData[i]) T(static_cast<T&&>(_data[i]));
            _data[i].~T();
        }

        freeRaw(_data);
        _data = newData;
        _capacity = newCap;
        return true;
    }
private:
    T* _data = nullptr;
    uint32_t _count = 0;
    uint32_t _capacity = 0;
};