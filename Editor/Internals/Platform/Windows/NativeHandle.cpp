#include "Editor/Internals/Platform/Windows/NativeHandle.h"
#include <windows.h>
#include <utility>

NativeHandle::NativeHandle() = default;
NativeHandle::NativeHandle(void* value)
    : _value(value) {}
NativeHandle::~NativeHandle() { Reset(); }
NativeHandle::NativeHandle(NativeHandle&& other) noexcept
    : _value(std::exchange(other._value, nullptr)) {}
NativeHandle& NativeHandle::operator=(NativeHandle&& other) noexcept {
    if (this != &other) Reset(std::exchange(other._value, nullptr));
    return *this;
}
NativeHandle::operator bool() const { return _value && _value != INVALID_HANDLE_VALUE; }
void NativeHandle::Reset(void* value) {
    if (*this) CloseHandle(_value);
    _value = value;
}
