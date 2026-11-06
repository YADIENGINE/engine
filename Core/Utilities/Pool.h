#pragma once
#include <algorithm>
#include <concepts>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <iterator>
#include <limits>
#include <memory>
#include <stdexcept>
#include <type_traits>
#include <utility>
#include <vector>
#include "Utilities/Reflection.h"

// Names one item of a Pool<T>, and stays valid while that item lives: removing other items does not change it, and once
// the item is removed the handle names nothing, even after its slot is used again. A Handle<Enemy> cannot be passed to
// a Pool<Bullet>. A plain struct: keep handles in the game's state (a target, a parent) and State writes them as
// fields. {} is the null handle.
template <class T> struct Handle {
    std::uint32_t index = 0;
    std::uint32_t generation = 0; // 0: names nothing

    [[nodiscard]] constexpr explicit operator bool() const { return generation != 0; }
    // The number to give a collider as its owner (CollisionWorld::Add); FromOwner turns it back. The null handle is 0.
    [[nodiscard]] constexpr std::uint64_t Owner() const { return (std::uint64_t{generation} << 32U) | index; }
    [[nodiscard]] static constexpr Handle FromOwner(std::uint64_t owner) {
        return {static_cast<std::uint32_t>(owner), static_cast<std::uint32_t>(owner >> 32U)};
    }
    friend constexpr bool operator==(Handle, Handle) = default;
};

// A list of things whose identity lasts: Add gives a handle, and the handle finds that item until it is removed. Items
// stay in spawn order (a removed item leaves, the others keep their order, new items go to the end), so loops and
// results replay the same way. The same operations give the same handles. A value: copies are independent and keep
// the same handles. Header-only, and a simulation may keep one in its state (state.Fields(_run) writes it).
//     struct Enemy { Float2 at; int hp = 3; Handle<Enemy> leader; };
//     const Handle<Enemy> boss = enemies.Add({.at = {400.0f, 80.0f}, .hp = 20});
//     for (Enemy& enemy : enemies) enemy.at.y += 1.0f;
//     if (Enemy* leader = enemies.Find(enemy.leader)) ...    // Null once the leader is gone
//     enemies.RemoveIf([](const Enemy& enemy) { return enemy.hp <= 0; });
// Adding or removing while a loop over the pool runs (or inside RemoveIf's question) throws std::logic_error; collect
// what to add and add it after the loop. A handle that names no item of this pool throws std::invalid_argument from
// Get and Remove. Two pools of one type may give equal handles, as two lists give equal indices: keep each handle with
// the pool that gave it. Colliders: Physics/Collision/PoolColliders.h.
template <class T> class Pool {
    static_assert(!std::is_same_v<T, bool>, "Pool<bool> would be std::vector<bool>; use a struct or std::uint8_t");
    template <bool Const> class Cursor;

public:
    using value_type = T;
    using iterator = Cursor<false>;
    using const_iterator = Cursor<true>;

    // Adds at the end of the order.
    Handle<T> Add(T item) {
        RequireNoLoop();
        const bool reuse = !_free.empty();
        if (!reuse && _generations.size() >= Free) throw std::length_error("Pool: too many slots");
        RoomForOneMore(_slotOf);
        if (!reuse) {
            RoomForOneMore(_generations);
            RoomForOneMore(_positions);
        }
        _items.push_back(std::move(item)); // The last step that can fail; nothing has changed before it
        std::uint32_t slot = 0;
        if (reuse) {
            slot = _free.back();
            _free.pop_back();
        } else {
            slot = static_cast<std::uint32_t>(_generations.size());
            _generations.push_back(1);
            _positions.push_back(Free);
        }
        _positions[slot] = static_cast<std::uint32_t>(_items.size() - 1);
        _slotOf.push_back(slot);
        return {slot, _generations[slot]};
    }
    // Removes one item; the items after it move up one place. For many at once, RemoveIf.
    void Remove(Handle<T> handle) {
        RequireNoLoop();
        const std::size_t position = PositionOf(handle);
        const std::uint32_t slot = _slotOf[position];
        _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(position));
        _slotOf.erase(_slotOf.begin() + static_cast<std::ptrdiff_t>(position));
        for (std::size_t i = position; i < _items.size(); ++i)
            _positions[_slotOf[i]] = static_cast<std::uint32_t>(i);
        Release(slot);
    }
    // Removes every item 'removes(item)' answers true for, in one pass, keeping the others' order; returns how many
    // left. Every item is asked before any leaves, so the question may still read any item through its handle.
    template <class Predicate>
        requires std::predicate<Predicate&, const T&>
    std::size_t RemoveIf(Predicate&& removes) {
        RequireNoLoop();
        std::vector<bool> leaving;
        {
            const Loop loop(*this);
            for (std::size_t i = 0; i < _items.size(); ++i) {
                if (!std::invoke(removes, std::as_const(_items[i]))) continue;
                if (leaving.empty()) leaving.resize(_items.size());
                leaving[i] = true;
            }
        }
        if (leaving.empty()) return 0;
        std::size_t kept = 0;
        for (std::size_t i = 0; i < _items.size(); ++i) {
            const std::uint32_t slot = _slotOf[i];
            if (leaving[i]) {
                Release(slot);
                continue;
            }
            if (kept != i) {
                _items[kept] = std::move(_items[i]);
                _slotOf[kept] = slot;
            }
            _positions[slot] = static_cast<std::uint32_t>(kept++);
        }
        const std::size_t removed = _items.size() - kept;
        _items.erase(_items.begin() + static_cast<std::ptrdiff_t>(kept), _items.end());
        _slotOf.resize(kept);
        return removed;
    }

    // The handle names an item of this pool.
    [[nodiscard]] bool Contains(Handle<T> handle) const {
        return handle.generation != 0 && handle.index < _generations.size() &&
               _generations[handle.index] == handle.generation && _positions[handle.index] != Free;
    }
    // The item; a handle that names none throws std::invalid_argument.
    [[nodiscard]] T& Get(Handle<T> handle) { return _items[PositionOf(handle)]; }
    [[nodiscard]] const T& Get(Handle<T> handle) const { return _items[PositionOf(handle)]; }
    // The item, or null when the handle names none (a target that is gone). Use the pointer at once and never keep
    // it: adding or removing moves items. Keep the handle instead.
    [[nodiscard]] T* Find(Handle<T> handle) { return Contains(handle) ? &_items[_positions[handle.index]] : nullptr; }
    [[nodiscard]] const T* Find(Handle<T> handle) const {
        return Contains(handle) ? &_items[_positions[handle.index]] : nullptr;
    }
    // The handle of an item reached through this pool (a loop, Get, Find): for (Enemy& e : enemies)
    // enemies.HandleOf(e). Anything else throws std::invalid_argument.
    [[nodiscard]] Handle<T> HandleOf(const T& item) const {
        const T* at = std::addressof(item);
        const T* first = _items.data();
        if (_items.empty() || std::less<const T*>{}(at, first) || !std::less<const T*>{}(at, first + _items.size()))
            throw std::invalid_argument("Pool: this item is not in the pool");
        return HandleAt(static_cast<std::size_t>(at - first));
    }
    [[nodiscard]] std::size_t Count() const { return _items.size(); }

    // Every item, in spawn order.
    [[nodiscard]] iterator begin() { return iterator(*this); }
    [[nodiscard]] const_iterator begin() const { return const_iterator(*this); }
    [[nodiscard]] std::default_sentinel_t end() const { return {}; }

    // Everything that decides later handles, for a simulation's State: count, then each item as name[i] with its
    // handle and its fields (enemies[0].handle.index, enemies[0].hp), then the slots' generations and the free slots in
    // the order they are used again.
    template <class Visitor> void State(Visitor& state) const {
        state.Field("count", _items.size());
        for (std::size_t i = 0; i < _items.size(); ++i) {
            state.Group({}, i, [&] {
                state.Field("handle", HandleAt(i));
                const T& item = _items[i];
                if constexpr (requires(Visitor& visitor) { item.State(visitor); })
                    item.State(state);
                else if constexpr (PlainStruct<T>)
                    state.Fields(item);
                else
                    state.Field("value", item);
            });
        }
        state.Field("generations", _generations);
        state.Field("free", _free);
    }

private:
    // A slot's position while it holds no item.
    static constexpr std::uint32_t Free = std::numeric_limits<std::uint32_t>::max();

    // How many loops over the pool are running; a copy of the pool has none.
    struct Loops {
        int count = 0;
        Loops() = default;
        Loops(const Loops&) noexcept {}
        Loops& operator=(const Loops&) noexcept { return *this; }
    };
    // Marks the pool as being looped over while it lives.
    class Loop {
    public:
        explicit Loop(const Pool& pool)
            : _pool(&pool) {
            ++_pool->_loops.count;
        }
        Loop(const Loop& other)
            : _pool(other._pool) {
            if (_pool) ++_pool->_loops.count;
        }
        Loop& operator=(const Loop& other) {
            if (other._pool) ++other._pool->_loops.count;
            if (_pool) --_pool->_loops.count;
            _pool = other._pool;
            return *this;
        }
        ~Loop() {
            if (_pool) --_pool->_loops.count;
        }
        Loop() = default;

    private:
        const Pool* _pool = nullptr;
    };

    template <bool Const> class Cursor {
        using Owner = std::conditional_t<Const, const Pool, Pool>;

    public:
        using value_type = T;
        using difference_type = std::ptrdiff_t;
        using reference = std::conditional_t<Const, const T&, T&>;
        using iterator_concept = std::forward_iterator_tag;

        Cursor() = default;
        explicit Cursor(Owner& pool)
            : _pool(&pool)
            , _loop(pool) {}

        [[nodiscard]] reference operator*() const { return _pool->_items[_position]; }
        Cursor& operator++() {
            ++_position;
            return *this;
        }
        Cursor operator++(int) {
            Cursor before = *this;
            ++_position;
            return before;
        }
        [[nodiscard]] friend bool operator==(const Cursor& a, const Cursor& b) { return a._position == b._position; }
        [[nodiscard]] friend bool operator==(const Cursor& cursor, std::default_sentinel_t) { return cursor.AtEnd(); }

    private:
        [[nodiscard]] bool AtEnd() const { return _pool == nullptr || _position >= _pool->_items.size(); }

        Owner* _pool = nullptr;
        Loop _loop;
        std::size_t _position = 0;
    };

    void RequireNoLoop() const {
        if (_loops.count != 0)
            throw std::logic_error("Pool: items cannot be added or removed while the pool is being looped over");
    }
    [[nodiscard]] std::size_t PositionOf(Handle<T> handle) const {
        if (!Contains(handle)) throw std::invalid_argument("Pool: this handle names no item of the pool");
        return _positions[handle.index];
    }
    [[nodiscard]] Handle<T> HandleAt(std::size_t position) const {
        const std::uint32_t slot = _slotOf[position];
        return {slot, _generations[slot]};
    }
    // Grows like push_back would, so that the push_back after it cannot fail.
    static void RoomForOneMore(std::vector<std::uint32_t>& list) {
        if (list.size() == list.capacity()) list.reserve(std::max<std::size_t>(8, list.capacity() * 2));
    }
    // A slot whose generation would wrap is never used again, so no old handle can name its next item.
    void Release(std::uint32_t slot) {
        _positions[slot] = Free;
        if (++_generations[slot] != 0) _free.push_back(slot);
    }

    std::vector<T> _items;                   // In spawn order
    std::vector<std::uint32_t> _slotOf;      // The slot of each item
    std::vector<std::uint32_t> _generations; // Per slot: the generation its handle carries; 0 when retired
    std::vector<std::uint32_t> _positions;   // Per slot: where its item is in _items, or Free
    std::vector<std::uint32_t> _free;        // Free slots; the last one is used first
    mutable Loops _loops;
};
