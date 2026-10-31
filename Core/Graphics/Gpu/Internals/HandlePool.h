#pragma once
#include <cstdint>
#include <optional>
#include <stdexcept>
#include <vector>
#include "Graphics/Gpu/Internals/GpuTypes.h"

// Objects addressed by generational handles. A handle to a removed object (or to a reused slot) throws logic_error
// instead of reaching the wrong object.
template <class Tag, class T> class HandlePool {
public:
    using Handle = GpuHandle<Tag>;

    [[nodiscard]] Handle Add(T value) {
        std::uint32_t index = 0;
        if (!_free.empty()) {
            index = _free.back();
            _free.pop_back();
        } else {
            index = static_cast<std::uint32_t>(_slots.size());
            _slots.emplace_back();
        }
        Slot& slot = _slots[index];
        ++slot.generation;
        slot.value = std::move(value);
        return {index, slot.generation};
    }
    [[nodiscard]] T& Get(Handle handle) { return Find(handle).value.value(); }
    [[nodiscard]] const T& Get(Handle handle) const { return const_cast<HandlePool*>(this)->Get(handle); }
    [[nodiscard]] bool Contains(Handle handle) const {
        return handle.index != 0 && handle.index < _slots.size() && _slots[handle.index].value &&
               _slots[handle.index].generation == handle.generation;
    }
    T Remove(Handle handle) {
        Slot& slot = Find(handle);
        T value = std::move(*slot.value);
        slot.value.reset();
        _free.push_back(handle.index);
        return value;
    }
    template <class Visit> void ForEach(Visit&& visit) {
        for (Slot& slot : _slots)
            if (slot.value) visit(*slot.value);
    }

private:
    struct Slot {
        std::uint32_t generation = 0;
        std::optional<T> value;
    };
    Slot& Find(Handle handle) {
        if (!Contains(handle)) throw std::logic_error("GPU handle is null, stale or from another device");
        return _slots[handle.index];
    }

    std::vector<Slot> _slots{1}; // Slot 0 is the null handle
    std::vector<std::uint32_t> _free;
};
