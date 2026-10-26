#pragma once

class NativeHandle {
public:
    NativeHandle();
    explicit NativeHandle(void* value);
    ~NativeHandle();
    NativeHandle(NativeHandle&& other) noexcept;
    NativeHandle& operator=(NativeHandle&& other) noexcept;
    [[nodiscard]] void* Get() const { return _value; }
    [[nodiscard]] explicit operator bool() const;
    void Reset(void* value = nullptr);

private:
    void* _value = nullptr;

public:
    NativeHandle(const NativeHandle&) = delete;
    NativeHandle& operator=(const NativeHandle&) = delete;
};
