#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <span>
#include <stdexcept>
#include <vector>

// The order of a 2.5D world pass (a y-sort): what stands lower on the screen is nearer, so it is drawn later. Each
// frame Add every thing with the y of the point it stands on (not where it shows: a jump does not bring it nearer) and
// a key the game reads back, then draw in Sorted() order. Things that stand level keep the order they were added in, so
// two of them never swap from one frame to the next. It holds keys only: no drawing, no pointers, no callbacks.
//
//     _order.Clear();                                   // A mutable member keeps its memory between frames
//     for (std::size_t i = 0; i < _enemies.size(); ++i) _order.Add(_enemies[i].ground.y, i);
//     for (std::size_t i : _order.Sorted()) DrawEnemy(paint, _enemies[i]);
//
// Several kinds of things share one order through a key of the game's own, such as a struct of a kind and an index.
// Header-only.
template <class Key = std::size_t> class DepthOrder {
public:
    void Clear() {
        _entries.clear();
        _sorted.clear();
    }
    // Throws std::invalid_argument for a depth that is not finite.
    void Add(float depth, const Key& key) {
        if (!std::isfinite(depth)) throw std::invalid_argument("DepthOrder: a depth must be finite");
        _entries.push_back({depth, _entries.size(), key});
    }
    // The keys from far to near; level ones in the order they were added since the last Clear.
    [[nodiscard]] std::span<const Key> Sorted() {
        std::ranges::sort(_entries, [](const Entry& a, const Entry& b) {
            return a.depth < b.depth || (a.depth == b.depth && a.added < b.added);
        });
        _sorted.clear();
        for (const Entry& entry : _entries)
            _sorted.push_back(entry.key);
        return _sorted;
    }

private:
    struct Entry {
        float depth;
        std::size_t added;
        Key key;
    };
    std::vector<Entry> _entries;
    std::vector<Key> _sorted;
};
