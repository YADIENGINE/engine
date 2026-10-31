#include "Graphics/Text/Internals/TextLayoutCache.h"
#include <bit>
#include <functional>
#include <iterator>

namespace {
constexpr std::uint64_t KeepFrames = 120; // A layout not drawn this long is dropped
constexpr std::size_t MaxEntries = 4096;

void Combine(std::size_t& hash, std::size_t value) {
    hash ^= value + 0x9e3779b97f4a7c15ULL + (hash << 6) + (hash >> 2);
}
} // namespace

TextLayoutCache::KeyView TextLayoutCache::View(const Key& key) {
    return {key.font,          key.size, key.maxWidth, key.lineSpacing, key.lineHeight,
            key.letterSpacing, key.wrap, key.fit,      key.text};
}

std::size_t TextLayoutCache::Hash::operator()(const KeyView& key) const noexcept {
    std::size_t hash = std::hash<std::string_view>{}(key.text);
    Combine(hash, key.font);
    Combine(hash, std::bit_cast<std::uint32_t>(key.size));
    Combine(hash, std::bit_cast<std::uint32_t>(key.maxWidth));
    Combine(hash, std::bit_cast<std::uint32_t>(key.lineSpacing));
    Combine(hash, std::bit_cast<std::uint32_t>(key.lineHeight));
    Combine(hash, std::bit_cast<std::uint32_t>(key.letterSpacing));
    Combine(hash, static_cast<std::size_t>(key.wrap) * 4 + static_cast<std::size_t>(key.fit));
    return hash;
}

std::size_t TextLayoutCache::Hash::operator()(const Key& key) const noexcept { return (*this)(View(key)); }

bool TextLayoutCache::Equal::operator()(const KeyView& a, const Key& b) const noexcept {
    return a.font == b.font && a.size == b.size && a.maxWidth == b.maxWidth && a.lineSpacing == b.lineSpacing &&
           a.lineHeight == b.lineHeight && a.letterSpacing == b.letterSpacing && a.wrap == b.wrap && a.fit == b.fit &&
           a.text == b.text;
}

const TextBlock& TextLayoutCache::Get(const FontFace& font, std::string_view text, const TextStyle& style) {
    const KeyView view{font.Id(),         style.size,       style.maxWidth,
                       style.lineSpacing, style.lineHeight, style.letterSpacing,
                       style.wrap,        style.fit,        text};
    if (const auto found = _entries.find(view); found != _entries.end()) {
        ++_hits;
        found->second.lastUsed = _frame;
        return found->second.block;
    }
    Key key{view.font,          view.size, view.maxWidth, view.lineSpacing, view.lineHeight,
            view.letterSpacing, view.wrap, view.fit,      std::string(text)};
    const auto& block = _entries.emplace(std::move(key), Entry{font.Layout(text, style), _frame}).first->second.block;
    ++_misses;
    return block;
}

void TextLayoutCache::EndFrame() {
    const bool full = _entries.size() > MaxEntries;
    std::erase_if(_entries,
                  [&](const auto& entry) { return _frame - entry.second.lastUsed > (full ? 0 : KeepFrames); });
    ++_frame;
    _hits = 0;
    _misses = 0;
}
