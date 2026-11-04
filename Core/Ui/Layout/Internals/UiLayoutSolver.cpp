#include "Ui/Layout/Internals/UiLayoutSolver.h"
#include <algorithm>
#include <cmath>
#include <vector>

namespace {
enum class Axis { X, Y };

[[nodiscard]] const UiSize& SizeOf(const UiBox& box, Axis axis) { return axis == Axis::X ? box.width : box.height; }
[[nodiscard]] float PaddingAlong(const UiBox& box, Axis axis) {
    return axis == Axis::X ? box.padding.left + box.padding.right : box.padding.top + box.padding.bottom;
}
[[nodiscard]] float Clamped(float value, const UiSize& size) {
    const float bounded = std::min(value, size.max);
    return std::max(std::max(bounded, size.min), 0.0f);
}
[[nodiscard]] bool Flows(const UiBox& parent, Axis axis) {
    return (parent.direction == UiDirection::Row && axis == Axis::X) ||
           (parent.direction == UiDirection::Column && axis == Axis::Y);
}
[[nodiscard]] float Gaps(const UiBox& box, int flowing) {
    return box.gap * static_cast<float>(std::max(flowing - 1, 0));
}

// Sizes are written to 'sizes' for one axis at a time; the other axis is already known when it is needed.
class Solver {
public:
    Solver(std::span<UiLayoutNode> nodes, const FloatRect& canvas)
        : _nodes(nodes)
        , _canvas(canvas)
        , _content(nodes.size())
        , _sizes(nodes.size()) {}

    void Run() {
        Axis axis = Axis::X;
        Content(axis);
        Assign(axis);
        for (std::size_t index = 0; index < _nodes.size(); ++index) _nodes[index].rect.width = _sizes[index];
        axis = Axis::Y;
        Content(axis);
        Assign(axis);
        for (std::size_t index = 0; index < _nodes.size(); ++index) _nodes[index].rect.height = _sizes[index];
        Place();
    }

private:
    [[nodiscard]] UiLayoutNode& Node(int index) { return _nodes[static_cast<std::size_t>(index)]; }
    [[nodiscard]] float& ContentOf(int index) { return _content[static_cast<std::size_t>(index)]; }
    [[nodiscard]] float& SizeAt(int index) { return _sizes[static_cast<std::size_t>(index)]; }

    // Children first: a node's wanted size comes from what its children want.
    void Content(Axis axis) {
        for (int index = static_cast<int>(_nodes.size()) - 1; index >= 0; --index) {
            UiLayoutNode& node = Node(index);
            const UiSize& size = SizeOf(node.box, axis);
            if (size.kind == UiSizeKind::Fixed) {
                ContentOf(index) = Clamped(size.value, size);
                continue;
            }
            float wanted = 0.0f;
            if (node.measure) {
                const float inner = std::max(node.rect.width - PaddingAlong(node.box, Axis::X), 0.0f);
                const float available = axis == Axis::X ? UiUnlimited : inner;
                const Float2 measured = (*node.measure)(available);
                wanted = axis == Axis::X ? measured.x : measured.y;
            }
            int flowing = 0;
            float sum = 0.0f;
            float largest = 0.0f;
            for (int child = node.firstChild; child >= 0; child = Node(child).nextSibling) {
                if (Node(child).box.anchor) continue;
                ++flowing;
                sum += ContentOf(child);
                largest = std::max(largest, ContentOf(child));
            }
            wanted = std::max(wanted, Flows(node.box, axis) ? sum + Gaps(node.box, flowing) : largest);
            ContentOf(index) = Clamped(wanted + PaddingAlong(node.box, axis), size);
        }
    }

    // Parents first: a node's final size comes from the room its parent has.
    void Assign(Axis axis) {
        for (int index = 0; index < static_cast<int>(_nodes.size()); ++index) {
            UiLayoutNode& node = Node(index);
            if (node.parent < 0) SizeAt(index) = axis == Axis::X ? _canvas.width : _canvas.height;
            const float room = std::max(SizeAt(index) - PaddingAlong(node.box, axis), 0.0f);
            if (Flows(node.box, axis)) {
                AssignAlong(node, axis, room);
            } else {
                AssignAcross(node, axis, room);
            }
        }
    }
    void AssignAlong(UiLayoutNode& node, Axis axis, float room) {
        float used = 0.0f;
        float weights = 0.0f;
        int flowing = 0;
        for (int child = node.firstChild; child >= 0; child = Node(child).nextSibling) {
            const UiLayoutNode& item = Node(child);
            if (item.box.anchor) {
                SizeAt(child) = Across(item, axis, child, room);
                continue;
            }
            ++flowing;
            const UiSize& size = SizeOf(item.box, axis);
            if (size.kind == UiSizeKind::Fill) {
                weights += std::max(size.value, 0.0f);
            } else {
                used += ContentOf(child);
            }
        }
        const float left = std::max(room - used - Gaps(node.box, flowing), 0.0f);
        for (int child = node.firstChild; child >= 0; child = Node(child).nextSibling) {
            const UiLayoutNode& item = Node(child);
            if (item.box.anchor) continue;
            const UiSize& size = SizeOf(item.box, axis);
            if (size.kind != UiSizeKind::Fill) {
                SizeAt(child) = ContentOf(child);
            } else {
                const float share = weights > 0.0f ? left * std::max(size.value, 0.0f) / weights : 0.0f;
                SizeAt(child) = Clamped(share, size);
            }
        }
    }
    void AssignAcross(UiLayoutNode& node, Axis axis, float room) {
        for (int child = node.firstChild; child >= 0; child = Node(child).nextSibling) {
            SizeAt(child) = Across(Node(child), axis, child, room);
        }
    }
    // Fill takes the room; Fit takes what it wants up to the room, which is how a long text comes to wrap.
    [[nodiscard]] float Across(const UiLayoutNode& item, Axis axis, int index, float room) {
        const UiSize& size = SizeOf(item.box, axis);
        switch (size.kind) {
        case UiSizeKind::Fixed: return ContentOf(index);
        case UiSizeKind::Fill : return Clamped(room, size);
        case UiSizeKind::Fit  : return Clamped(std::min(ContentOf(index), room), size);
        }
        return ContentOf(index);
    }

    void Place() {
        for (int index = 0; index < static_cast<int>(_nodes.size()); ++index) {
            UiLayoutNode& node = Node(index);
            if (node.parent < 0) {
                node.rect.x = _canvas.x;
                node.rect.y = _canvas.y;
            }
            PlaceChildren(node);
        }
    }
    void PlaceChildren(UiLayoutNode& node) {
        const FloatRect inner{node.rect.x + node.box.padding.left, node.rect.y + node.box.padding.top,
                              std::max(node.rect.width - PaddingAlong(node.box, Axis::X), 0.0f),
                              std::max(node.rect.height - PaddingAlong(node.box, Axis::Y), 0.0f)};
        const Axis main = node.box.direction == UiDirection::Row ? Axis::X : Axis::Y;
        const UiAlign mainAlign = main == Axis::X ? node.box.alignX : node.box.alignY;
        const UiAlign crossAlign = main == Axis::X ? node.box.alignY : node.box.alignX;
        const bool stacked = node.box.direction == UiDirection::Overlay;
        float total = 0.0f;
        int flowing = 0;
        for (int child = node.firstChild; child >= 0; child = Node(child).nextSibling) {
            const UiLayoutNode& item = Node(child);
            if (item.box.anchor) continue;
            ++flowing;
            total += main == Axis::X ? item.rect.width : item.rect.height;
        }
        total += Gaps(node.box, flowing);
        const float mainStart = main == Axis::X ? inner.x : inner.y;
        const float mainRoom = main == Axis::X ? inner.width : inner.height;
        float cursor = mainStart + AlignedOffset(mainAlign, mainRoom, total);
        for (int child = node.firstChild; child >= 0; child = Node(child).nextSibling) {
            UiLayoutNode& item = Node(child);
            if (item.box.anchor) {
                PlaceAnchored(item, inner);
                continue;
            }
            const float mainSize = main == Axis::X ? item.rect.width : item.rect.height;
            const float crossSize = main == Axis::X ? item.rect.height : item.rect.width;
            const float crossStart = main == Axis::X ? inner.y : inner.x;
            const float crossRoom = main == Axis::X ? inner.height : inner.width;
            const float along = stacked ? mainStart + AlignedOffset(mainAlign, mainRoom, mainSize) : cursor;
            const float across = crossStart + AlignedOffset(crossAlign, crossRoom, crossSize);
            item.rect.x = main == Axis::X ? along : across;
            item.rect.y = main == Axis::X ? across : along;
            if (!stacked) cursor += mainSize + node.box.gap;
        }
    }
    [[nodiscard]] static float AlignedOffset(UiAlign align, float room, float size) {
        switch (align) {
        case UiAlign::Start : return 0.0f;
        case UiAlign::Center: return (room - size) * 0.5f;
        case UiAlign::End   : return room - size;
        }
        return 0.0f;
    }
    static void PlaceAnchored(UiLayoutNode& item, const FloatRect& inner) {
        const auto column = static_cast<int>(*item.box.anchor) % 3;
        const auto row = static_cast<int>(*item.box.anchor) / 3;
        const auto Share = [](int place) { return static_cast<float>(place) * 0.5f; };
        item.rect.x = inner.x + (inner.width - item.rect.width) * Share(column) + item.box.offset.x;
        item.rect.y = inner.y + (inner.height - item.rect.height) * Share(row) + item.box.offset.y;
    }

    std::span<UiLayoutNode> _nodes;
    FloatRect _canvas;
    std::vector<float> _content;
    std::vector<float> _sizes;
};
} // namespace

void SolveUiLayout(std::span<UiLayoutNode> nodes, const FloatRect& canvas) { Solver(nodes, canvas).Run(); }
