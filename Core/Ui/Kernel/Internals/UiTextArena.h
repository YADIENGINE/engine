#pragma once
#include <algorithm>
#include <cstddef>
#include <cstring>
#include <memory>
#include <string_view>
#include <vector>

// Copies of this frame's texts. A copy keeps its address until Reset, and blocks are kept for the next frame.
class UiTextArena {
public:
    [[nodiscard]] std::string_view Copy(std::string_view text) {
        if (text.empty()) return {};
        if (_block == _blocks.size() || _used + text.size() > _blocks[_block].size) Advance(text.size());
        char* target = _blocks[_block].data.get() + _used;
        std::memcpy(target, text.data(), text.size());
        _used += text.size();
        return {target, text.size()};
    }
    void Reset() {
        _block = 0;
        _used = 0;
    }

private:
    static constexpr std::size_t BlockSize = 16 * 1024;
    struct Block {
        std::unique_ptr<char[]> data;
        std::size_t size = 0;
    };
    void Advance(std::size_t needed) {
        if (_block < _blocks.size()) ++_block;
        while (_block < _blocks.size() && _blocks[_block].size < needed) ++_block;
        _used = 0;
        if (_block == _blocks.size()) {
            const std::size_t size = std::max(BlockSize, needed);
            _blocks.push_back({std::make_unique<char[]>(size), size});
        }
    }

    std::vector<Block> _blocks;
    std::size_t _block = 0;
    std::size_t _used = 0;
};
