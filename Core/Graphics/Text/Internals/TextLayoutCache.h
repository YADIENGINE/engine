#pragma once
#include <cstdint>
#include <string>
#include <string_view>
#include <unordered_map>
#include "Graphics/Text/FontFace.h"

// Layouts of strings drawn with Painter::Text, so a label drawn every frame is shaped once. Entries not drawn for a
// while are dropped (EndFrame).
class TextLayoutCache {
public:
    TextLayoutCache() = default;

    [[nodiscard]] const TextBlock& Get(const FontFace& font, std::string_view text, const TextStyle& style);
    void EndFrame();
    [[nodiscard]] std::size_t Size() const { return _entries.size(); }
    [[nodiscard]] std::uint32_t HitsThisFrame() const { return _hits; }
    [[nodiscard]] std::uint32_t MissesThisFrame() const { return _misses; }

private:
    // The style fields layout depends on, plus the text.
    struct KeyView {
        std::uint32_t font;
        float size;
        float maxWidth;
        float lineSpacing;
        float lineHeight;
        float letterSpacing;
        TextWrap wrap;
        TextFit fit;
        std::string_view text;
    };
    struct Key {
        std::uint32_t font;
        float size;
        float maxWidth;
        float lineSpacing;
        float lineHeight;
        float letterSpacing;
        TextWrap wrap;
        TextFit fit;
        std::string text;
    };
    struct Hash {
        using is_transparent = void;
        std::size_t operator()(const KeyView& key) const noexcept;
        std::size_t operator()(const Key& key) const noexcept;
    };
    struct Equal {
        using is_transparent = void;
        bool operator()(const KeyView& a, const Key& b) const noexcept;
        bool operator()(const Key& a, const KeyView& b) const noexcept { return (*this)(b, a); }
        bool operator()(const Key& a, const Key& b) const noexcept { return (*this)(View(a), b); }
    };
    struct Entry {
        TextBlock block;
        std::uint64_t lastUsed = 0;
    };
    [[nodiscard]] static KeyView View(const Key& key);

    std::unordered_map<Key, Entry, Hash, Equal> _entries;
    std::uint64_t _frame = 0;
    std::uint32_t _hits = 0;
    std::uint32_t _misses = 0;
};
