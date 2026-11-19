#pragma once

template <typename HandleType, typename Deleter>
struct HandleTraits {
    static constexpr HandleType Invalid() noexcept {
        if constexpr (requires { Deleter::Invalid(); }) {
            return Deleter::Invalid();
        } else {
            return nullptr;
        }
    }
};

template <typename HandleType, typename Deleter>
class UniqueHandle {
    using Traits = HandleTraits<HandleType, Deleter>;

public:
    explicit UniqueHandle(HandleType handle = Traits::Invalid()) noexcept 
        : _handle(handle) {}

    ~UniqueHandle() noexcept { reset(); }

    UniqueHandle(const UniqueHandle&) = delete;
    UniqueHandle& operator=(const UniqueHandle&) = delete;

    UniqueHandle(UniqueHandle&& other) noexcept : _handle(other._handle) {
        other._handle = Traits::Invalid();
    }

    UniqueHandle& operator=(UniqueHandle&& other) noexcept {
        if (this != &other) {
            reset();
            _handle = other._handle;
            other._handle = Traits::Invalid();
        }
        return *this;
    }

    void reset(HandleType newHandle = Traits::Invalid()) noexcept {
        if (_handle != Traits::Invalid()) {
            Deleter::Close(_handle);
        }
        _handle = newHandle;
    }

    HandleType operator->() const noexcept { return _handle; }
    HandleType get() const noexcept { return _handle; }
    explicit operator bool() const noexcept { return _handle != Traits::Invalid(); }

    HandleType release() noexcept {
        HandleType temp = _handle;
        _handle = Traits::Invalid();
        return temp;
    }

private:
    HandleType _handle;
};
