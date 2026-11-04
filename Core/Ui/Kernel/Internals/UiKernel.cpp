#include "Ui/Kernel/Internals/UiKernel.h"
#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <optional>
#include <stdexcept>
#include <string>
#include "Graphics/Paint/Painter.h"
#include "Ui/Kernel/UiLayerScope.h"
#include "Utilities/Affine2.h"
#include "Utilities/DevelopmentChecks.h"

namespace {
constexpr UiId BaseLayer{"UiBaseLayer"};
constexpr float DefaultLayerSeconds = 0.15f;

using KeyStates = std::array<bool, static_cast<std::size_t>(InputKey::Count)>;
[[nodiscard]] bool Any(const std::vector<InputKey>& keys, const KeyStates& state) {
    return std::ranges::any_of(keys, [&](InputKey key) { return state[static_cast<std::size_t>(key)]; });
}
[[nodiscard]] Float2 ToFloat(Vec2 point) { return {static_cast<float>(point.x), static_cast<float>(point.y)}; }
[[nodiscard]] bool Same(const std::optional<UiId>& left, UiId right) { return left && *left == right; }
[[nodiscard]] bool Captures(UiCapture capture, UiMove move) {
    const bool sideways = move == UiMove::Left || move == UiMove::Right;
    return capture == UiCapture::Both || (capture == UiCapture::Horizontal && sideways) ||
           (capture == UiCapture::Vertical && !sideways);
}
[[nodiscard]] UiStep StepOf(UiMove move, int user) {
    switch (move) {
    case UiMove::Up   : return {0, -1, user};
    case UiMove::Down : return {0, 1, user};
    case UiMove::Left : return {-1, 0, user};
    case UiMove::Right: return {1, 0, user};
    }
    return {};
}
} // namespace

UiKernel::UiKernel(const UiSettings& given)
    : settings(given)
    , looks(ActiveUiLooks()) {
    SetUsers({UiKeys::All()});
}

void UiKernel::SetUsers(std::vector<UiKeys> users) {
    if (users.empty() || static_cast<int>(users.size()) > MaxUsers) {
        throw std::invalid_argument("Ui::SetUsers: needs at least one player and at most 32");
    }
    _users = std::move(users);
    _userInput.assign(_users.size(), {});
    for (auto& [id, memory] : _layerMemory) {
        memory.focus.assign(_users.size(), {});
        memory.focusRect.assign(_users.size(), {});
        memory.hasFocus.assign(_users.size(), false);
    }
}
void UiKernel::Input(const DeviceInput& input) { _input = input; }
void UiKernel::CancelInput() {
    _input.reset();
    _pressed.reset();
    for (UiUserInput& user : _userInput) {
        user.held.reset();
        user.confirmDown = false;
    }
}
const FontFace& UiKernel::Font() const {
    if (!settings.font) throw std::logic_error("Ui: text needs a font (UiSettings::font)");
    return *settings.font;
}
void UiKernel::CheckBuilding(const char* call) const {
    if (!_building) throw std::logic_error(std::string(call) + ": only between Ui::Begin and Ui::End");
}
void UiKernel::Report(const char* message) {
    if (DevelopmentChecks()) throw std::logic_error(message);
    ++stats.errors;
}

// --- The frame --------------------------------------------------------------------------------------------------

void UiKernel::Begin(float seconds) {
    if (_building) throw std::logic_error("Ui::Begin: the last frame was not ended");
    _building = true;
    _built = false;
    ++_frame;
    _delta = std::isfinite(seconds) ? std::max(seconds, 0.0f) : 0.0f;
    _nodes.clear();
    _layout.clear();
    _layers.clear();
    _events.clear();
    _activated.clear();
    _steps.clear();
    _denied.clear();
    _wheel = 0.0f;
    _pageStep = 0;
    _backUsers.clear();
    _text.Reset();
    _seen = 0;
    _scopeError = nullptr;
    ResolveInput();
    _input.reset();

    UiLayerMemory& memory = MemoryOf(BaseLayer);
    memory.lastFrame = _frame;
    _layers.push_back({BaseLayer, 0, false, false});
    _cursor = -1;
    _cursorLayer = 0;
    _layers[0].root = OpenElement(BaseLayer, {.width = UiFill(), .height = UiFill()}, {});
}

void UiKernel::End() {
    if (!_building) throw std::logic_error("Ui::End: the frame was not begun");
    if (_scopeError) {
        const char* message = _scopeError;
        _scopeError = nullptr;
        _building = false;
        throw std::logic_error(message);
    }
    if (_cursor != 0 || _cursorLayer != 0) Report("Ui::End: an element or a layer is still open");
    _building = false;
    for (std::size_t index = 0; index < _nodes.size(); ++index) {
        _layout[index].measure = _nodes[index].measure ? &_nodes[index].measure : nullptr;
    }
    SolveUiLayout(_layout, settings.canvas);
    ComputeVisibleRects();
    CollectHits();
    _lastLayers = _layers;
    UpdateScopes();
    RepairFocus();
    SnapshotDrawStates();
    ForgetUnseen();
    stats.elements = static_cast<int>(_nodes.size());
    stats.interactive = static_cast<int>(_hits.size());
    stats.stateEntries = static_cast<int>(_entries.size());
    _built = true;
}

void UiKernel::Draw(Painter& paint) const {
    if (!_built) return;
    for (const UiLayerRecord& layer : _layers) DrawNode(paint, layer.root);
}

void UiKernel::DrawNode(Painter& paint, int index) const {
    const UiNode& node = Node(index);
    const UiLayoutNode& placed = LayoutNode(index);
    const UiVisual& visual = node.visual;
    if (!(visual.opacity > 0.0f)) return;
    std::optional<PaintScope> transform;
    std::optional<PaintScope> opacity;
    std::optional<PaintScope> clip;
    if (visual.offset != Float2{} || visual.scale != 1.0f) {
        transform.emplace(paint.Transform(Affine2::Translate(visual.offset) *
                                          Affine2::Scale(visual.scale, placed.rect.Center())));
    }
    if (visual.opacity < 1.0f) opacity.emplace(paint.Opacity(visual.opacity));
    if (node.clip) clip.emplace(paint.Clip(placed.rect));
    if (node.draw) node.draw(paint, node.state);
    for (int child = placed.firstChild; child >= 0; child = LayoutNode(child).nextSibling) DrawNode(paint, child);
}

// --- Describing a frame -------------------------------------------------------------------------------------------

int UiKernel::OpenElement(std::optional<UiId> given, const UiBox& box, const UiTraits& traits) {
    CheckBuilding("Ui::Element");
    const int parent = _cursor;
    const int index = static_cast<int>(_nodes.size());
    UiId id = BaseLayer;
    if (parent >= 0) {
        UiNode& up = Node(parent);
        // A place among siblings cannot collide with a given name in practice: it is hashed with a marker.
        const UiId local = given ? *given : UiId(std::uint64_t{0x5EED0000} + static_cast<std::uint64_t>(up.children));
        id = local.Under(up.id);
        ++up.children;
    } else if (given) {
        id = *given;
    }
    auto [found, created] = _entries.try_emplace(id);
    UiEntry& entry = found->second;
    if (!created && entry.lastFrame == _frame) {
        Report("Ui::Element: two elements have the same id under one parent; give them different ids");
        id = UiId(static_cast<std::uint64_t>(index)).Under(id);
        return OpenElement(id, box, traits);
    }
    entry.age = created ? 0.0f : entry.age + _delta;
    entry.lastFrame = _frame;
    ++_seen;

    UiNode& node = _nodes.emplace_back();
    node.id = id;
    node.entry = &entry;
    node.layer = _cursorLayer;
    node.owner = traits.owner != UiAnyUser || parent < 0 ? traits.owner : Node(parent).owner;
    node.interact = traits.interact;
    node.capture = traits.capture;
    node.enabled = traits.enabled;
    node.clip = traits.clip;
    node.visual = traits.visual;
    node.measure = traits.measure;

    UiLayoutNode& placed = _layout.emplace_back();
    placed.box = box;
    placed.parent = parent;
    if (parent >= 0) {
        UiLayoutNode& up = LayoutNode(parent);
        if (up.lastChild >= 0) {
            LayoutNode(up.lastChild).nextSibling = index;
        } else {
            up.firstChild = index;
        }
        up.lastChild = index;
    }
    _cursor = index;
    return index;
}

void UiKernel::CloseElement(int node) noexcept {
    if (!_building) return;
    if (_cursor != node) {
        _scopeError = "Ui: element scopes ended out of order; keep them in local variables, or call Close()";
        return;
    }
    _cursor = LayoutNode(node).parent;
}

bool UiKernel::OpenLayer(UiId given, bool open, const UiLayerSpec& spec, float& progress, int& root, int& savedCursor,
                         int& savedLayer) {
    CheckBuilding("Ui::Layer");
    const UiId id = given.Under(BaseLayer);
    UiLayerMemory& memory = MemoryOf(id);
    memory.lastFrame = _frame;
    if (open && !memory.wasOpen) {
        // A new opening starts its cursors over.
        memory.hasFocus.assign(_users.size(), false);
        if (spec.modal) Emit(UiEventKind::LayerOpened, id, 0, {});
    }
    if (!open && memory.wasOpen && spec.modal) Emit(UiEventKind::LayerClosed, id, 0, {});
    memory.wasOpen = open;
    const float target = open ? 1.0f : 0.0f;
    const float seconds = spec.seconds.value_or(DefaultLayerSeconds);
    if (seconds > 0.0f) {
        const float step = _delta / seconds;
        memory.progress = memory.progress < target ? std::min(memory.progress + step, target)
                                                   : std::max(memory.progress - step, target);
    } else {
        memory.progress = target;
    }
    progress = memory.progress;
    if (!open && memory.progress <= 0.0f) return false;

    savedCursor = _cursor;
    savedLayer = _cursorLayer;
    const auto layerIndex = static_cast<int>(_layers.size());
    _layers.push_back({id, -1, spec.modal, !open});
    _cursorLayer = layerIndex;
    _cursor = -1;
    root = OpenElement(id,
                       {.width = UiFill(),
                        .height = UiFill(),
                        .direction = spec.direction,
                        .gap = spec.gap,
                        .padding = spec.padding,
                        .alignX = spec.alignX,
                        .alignY = spec.alignY},
                       {});
    _layers[static_cast<std::size_t>(layerIndex)].root = root;
    return true;
}

void UiKernel::CloseLayer(int savedCursor, int savedLayer) noexcept {
    if (!_building) return;
    const int root = _layers[static_cast<std::size_t>(_cursorLayer)].root;
    if (_cursor != root) {
        _scopeError = "Ui: a layer ended while an element inside it was open";
        return;
    }
    _cursor = savedCursor;
    _cursorLayer = savedLayer;
}

UiAction UiKernel::Back() const {
    CheckBuilding("Ui::Back");
    if (_backUsers.empty() || !DescribingTopLayer()) return {};
    return {true, _backUsers.front()};
}
int UiKernel::PageStep() const {
    CheckBuilding("Ui::PageStep");
    return DescribingTopLayer() ? _pageStep : 0;
}
bool UiKernel::DescribingTopLayer() const {
    if (_lastLayers.empty()) return false;
    return _lastLayers[FirstInteractiveLayer()].id == _layers[static_cast<std::size_t>(_cursorLayer)].id;
}

// --- Answers ------------------------------------------------------------------------------------------------------

bool UiKernel::Hovered(int index) const {
    const UiNode& node = Node(index);
    return !ClosingLayer(node.layer) && Same(_hot, node.id);
}
bool UiKernel::Pressed(int index) const {
    const UiNode& node = Node(index);
    if (ClosingLayer(node.layer)) return false;
    if (Same(_pressed, node.id) && Same(_hot, node.id)) return true;
    for (int user = 0; user < UserCount(); ++user) {
        if (_userInput[static_cast<std::size_t>(user)].confirmDown && Focused(index, user)) return true;
    }
    return false;
}
bool UiKernel::Held(int index) const {
    const UiNode& node = Node(index);
    return !ClosingLayer(node.layer) && Same(_pressed, node.id);
}
bool UiKernel::PointerInside(int index) const {
    const UiNode& node = Node(index);
    if (ClosingLayer(node.layer) || !_pointer || !node.entry->visible.Contains(*_pointer)) return false;
    const UiId layer = _layers[static_cast<std::size_t>(node.layer)].id;
    for (std::size_t last = FirstInteractiveLayer(); last < _lastLayers.size(); ++last) {
        if (_lastLayers[last].id == layer) return !_lastLayers[last].closing;
    }
    return false;
}
bool UiKernel::Focused(int index, int user) const {
    const UiNode& node = Node(index);
    if (ClosingLayer(node.layer)) return false;
    const UiId layer = _layers[static_cast<std::size_t>(node.layer)].id;
    const auto memory = _layerMemory.find(layer);
    if (memory == _layerMemory.end()) return false;
    const auto holds = [&](int who) {
        const auto slot = static_cast<std::size_t>(who);
        return Same(_userInput[slot].scopeLayer, layer) && memory->second.hasFocus[slot] &&
               memory->second.focus[slot] == node.id;
    };
    if (user != UiAnyUser) return user >= 0 && user < UserCount() && holds(user);
    for (int who = 0; who < UserCount(); ++who) {
        if (holds(who)) return true;
    }
    return false;
}
UiAction UiKernel::Activated(int index) const {
    const UiNode& node = Node(index);
    if (!ClosingLayer(node.layer)) {
        for (const Outcome& outcome : _activated) {
            if (outcome.id == node.id) return {true, outcome.user, outcome.byPointer};
        }
    }
    return {};
}
UiStep UiKernel::Step(int index) const {
    const UiNode& node = Node(index);
    if (!ClosingLayer(node.layer)) {
        for (const StepOutcome& outcome : _steps) {
            if (outcome.id == node.id) return outcome.step;
        }
    }
    return {};
}
bool UiKernel::Denied(int index) const {
    const UiNode& node = Node(index);
    return !ClosingLayer(node.layer) && std::ranges::find(_denied, node.id) != _denied.end();
}
FloatRect UiKernel::LastRect(int index) const { return Node(index).entry->visible; }
Float2 UiKernel::LastSize(int index) const { return Node(index).entry->size; }
std::optional<FloatRect> UiKernel::FocusInside(int index) const { return Node(index).entry->focusInside; }
float UiKernel::Age(int index) const { return Node(index).entry->age; }
void UiKernel::TakeFocus(int index, int user) {
    const UiNode& node = Node(index);
    UiLayerMemory& memory = MemoryOf(_layers[static_cast<std::size_t>(node.layer)].id);
    const auto slot = static_cast<std::size_t>(user);
    memory.focus.at(slot) = node.id;
    memory.focusRect[slot] = node.entry->full;
    memory.hasFocus[slot] = true;
    _keysMovedFrame = _frame;
}
void* UiKernel::FindState(int index, std::type_index type) const {
    for (const StateSlot& slot : Node(index).entry->states) {
        if (slot.type == type) return slot.value.get();
    }
    return nullptr;
}
void* UiKernel::AddState(int index, std::type_index type, std::shared_ptr<void> state) {
    return Node(index).entry->states.emplace_back(StateSlot{type, std::move(state)}).value.get();
}

// --- Reading the input ----------------------------------------------------------------------------------------------

void UiKernel::ResolveInput() {
    if (!_input) {
        for (UiUserInput& user : _userInput) {
            user.held.reset();
            user.confirmDown = false;
        }
        return;
    }
    _wheel = _input->wheel;
    ResolvePointer(*_input);
    ResolveKeys(*_input);
}

// Layers from this index up take input: everything above the topmost open modal one, and that one.
std::size_t UiKernel::FirstInteractiveLayer() const {
    for (std::size_t layer = _lastLayers.size(); layer-- > 0;) {
        if (_lastLayers[layer].modal && !_lastLayers[layer].closing) return layer;
    }
    return 0;
}
bool UiKernel::PointerMayUse(const UiHit& hit) const { return UserCount() == 1 || hit.owner == UiAnyUser; }
bool UiKernel::UserMayFocus(const UiHit& hit, int user) const {
    return hit.interact == UiInteract::Focus && (hit.owner == UiAnyUser || hit.owner == user);
}
const UiKernel::UiHit* UiKernel::HitAt(Float2 point) const {
    const auto first = static_cast<int>(FirstInteractiveLayer());
    for (auto hit = _hits.rbegin(); hit != _hits.rend(); ++hit) {
        if (hit->layer < first) break;
        if (_lastLayers[static_cast<std::size_t>(hit->layer)].closing || !hit->visible.Contains(point)) continue;
        return PointerMayUse(*hit) ? &*hit : nullptr;
    }
    return nullptr;
}

void UiKernel::ResolvePointer(const DeviceInput& input) {
    _hot.reset();
    if (input.mousePosition) _pointer = ToFloat(*input.mousePosition);
    if (input.mousePosition) {
        if (const UiHit* hit = HitAt(ToFloat(*input.mousePosition))) _hot = hit->id;
    }
    if (input.mousePress) {
        _pressed.reset();
        if (const UiHit* hit = HitAt(ToFloat(*input.mousePress))) {
            _pressed = hit->id;
            if (hit->interact == UiInteract::Focus) MoveFocus(*hit, 0, true);
        }
    }
    if (input.mouseReleased && _pressed) {
        const UiHit* hit = input.mousePosition ? HitAt(ToFloat(*input.mousePosition)) : nullptr;
        if (hit && hit->id == *_pressed) Activate(*hit, 0, true);
        _pressed.reset();
    } else if (!input.mouseDown && !input.mousePress) {
        _pressed.reset();
    }
    if (input.mouseHover && !_pressed) {
        const UiHit* hit = HitAt(ToFloat(*input.mouseHover));
        if (hit && hit->interact == UiInteract::Focus) MoveFocus(*hit, 0, true);
    }
}

std::optional<UiMove> UiKernel::PressedMove(const UiKeys& keys, const DeviceInput& input, UiUserInput& state) {
    const auto pressed = [&](const std::vector<InputKey>& list) { return Any(list, input.keysPressed); };
    const auto down = [&](const std::vector<InputKey>& list) { return Any(list, input.keysDown); };
    std::optional<UiMove> fresh;
    if (pressed(keys.up)) fresh = UiMove::Up;
    if (pressed(keys.down)) fresh = UiMove::Down;
    if (pressed(keys.left)) fresh = UiMove::Left;
    if (pressed(keys.right)) fresh = UiMove::Right;
    if (fresh) {
        state.held = fresh;
        state.heldSeconds = 0.0f;
        state.nextRepeat = settings.repeatDelay;
        return fresh;
    }
    if (!state.held) return std::nullopt;
    const std::vector<InputKey>& heldKeys = *state.held == UiMove::Up     ? keys.up
                                            : *state.held == UiMove::Down ? keys.down
                                            : *state.held == UiMove::Left ? keys.left
                                                                          : keys.right;
    if (!down(heldKeys)) {
        state.held.reset();
        return std::nullopt;
    }
    state.heldSeconds += _delta;
    if (state.heldSeconds < state.nextRepeat || !(settings.repeatSeconds > 0.0f)) return std::nullopt;
    state.nextRepeat += settings.repeatSeconds;
    return state.held;
}

void UiKernel::ResolveKeys(const DeviceInput& input) {
    UpdateScopes();
    for (int user = 0; user < UserCount(); ++user) {
        const auto slot = static_cast<std::size_t>(user);
        const UiKeys& keys = _users[slot];
        UiUserInput& state = _userInput[slot];
        if (const std::optional<UiMove> move = PressedMove(keys, input, state)) {
            const UiHit* focused = FocusedHit(user);
            if (focused && Captures(focused->capture, *move)) {
                if (focused->enabled) {
                    _steps.push_back({focused->id, StepOf(*move, user)});
                    Emit(UiEventKind::Stepped, focused->id, user, focused->full);
                } else {
                    _denied.push_back(focused->id);
                    Emit(UiEventKind::Denied, focused->id, user, focused->full);
                }
            } else {
                MoveFocusBy(user, *move);
            }
        }
        if (Any(keys.previous, input.keysPressed)) _pageStep = -1;
        if (Any(keys.next, input.keysPressed)) _pageStep = 1;
        state.confirmDown = Any(keys.confirm, input.keysDown);
        if (Any(keys.back, input.keysPressed)) {
            _backUsers.push_back(user);
            Emit(UiEventKind::Back, {}, user, {});
        }
        if (!Any(keys.confirm, input.keysPressed)) continue;
        if (const UiHit* focused = FocusedHit(user)) Activate(*focused, user, false);
    }
}

const UiKernel::UiHit* UiKernel::FocusedHit(int user) const {
    const auto slot = static_cast<std::size_t>(user);
    const std::optional<UiId>& scope = _userInput[slot].scopeLayer;
    if (!scope) return nullptr;
    const auto memory = _layerMemory.find(*scope);
    if (memory == _layerMemory.end() || !memory->second.hasFocus[slot]) return nullptr;
    const UiId current = memory->second.focus[slot];
    const auto hit =
        std::ranges::find_if(_hits, [&](const UiHit& item) { return item.id == current && UserMayFocus(item, user); });
    return hit == _hits.end() ? nullptr : &*hit;
}

std::optional<std::size_t> UiKernel::ScopeLayerOf(int user) const {
    const std::size_t first = FirstInteractiveLayer();
    for (std::size_t layer = _lastLayers.size(); layer-- > first;) {
        if (_lastLayers[layer].closing) continue;
        for (const UiHit& hit : _hits) {
            if (hit.layer == static_cast<int>(layer) && UserMayFocus(hit, user)) return layer;
        }
    }
    return std::nullopt;
}
void UiKernel::UpdateScopes() {
    for (int user = 0; user < UserCount(); ++user) {
        const std::optional<std::size_t> layer = ScopeLayerOf(user);
        _userInput[static_cast<std::size_t>(user)].scopeLayer =
            layer ? std::optional<UiId>(_lastLayers[*layer].id) : std::nullopt;
    }
}

void UiKernel::MoveFocus(const UiHit& hit, int user, bool announce) {
    UiLayerMemory& memory = MemoryOf(_lastLayers[static_cast<std::size_t>(hit.layer)].id);
    const auto slot = static_cast<std::size_t>(user);
    const bool changed = !memory.hasFocus[slot] || memory.focus[slot] != hit.id;
    memory.focus[slot] = hit.id;
    memory.focusRect[slot] = hit.full;
    memory.hasFocus[slot] = true;
    if (changed && announce) Emit(UiEventKind::FocusMoved, hit.id, user, hit.full);
}

void UiKernel::MoveFocusBy(int user, UiMove move) {
    const auto slot = static_cast<std::size_t>(user);
    const std::optional<UiId>& scope = _userInput[slot].scopeLayer;
    if (!scope) return;
    const UiLayerMemory& memory = MemoryOf(*scope);
    const FloatRect from = memory.focusRect[slot];
    const UiId current = memory.focus[slot];
    const bool hasCurrent = memory.hasFocus[slot];
    std::vector<const UiHit*> items;
    std::vector<FloatRect> rects;
    for (const UiHit& hit : _hits) {
        const bool inScope = _lastLayers[static_cast<std::size_t>(hit.layer)].id == *scope;
        if (!inScope || !UserMayFocus(hit, user) || (hasCurrent && hit.id == current)) continue;
        items.push_back(&hit);
        rects.push_back(hit.full);
    }
    if (items.empty()) return;
    if (!hasCurrent) {
        MoveFocus(*items.front(), user, true);
        _keysMovedFrame = _frame;
        return;
    }
    if (const auto target = FindUiFocusTarget(rects, from, move, settings.wrapFocus)) {
        MoveFocus(*items[*target], user, true);
        _keysMovedFrame = _frame;
    }
}

void UiKernel::Activate(const UiHit& hit, int user, bool byPointer) {
    if (hit.enabled) {
        _activated.push_back({hit.id, user, byPointer});
        Emit(UiEventKind::Activated, hit.id, user, hit.full);
    } else {
        _denied.push_back(hit.id);
        Emit(UiEventKind::Denied, hit.id, user, hit.full);
    }
}
void UiKernel::Emit(UiEventKind kind, UiId id, int user, const FloatRect& rect) {
    _events.push_back({kind, id, user, rect});
}
UiKernel::UiLayerMemory& UiKernel::MemoryOf(UiId layer) {
    auto [found, created] = _layerMemory.try_emplace(layer);
    if (created) {
        found->second.focus.assign(_users.size(), {});
        found->second.focusRect.assign(_users.size(), {});
        found->second.hasFocus.assign(_users.size(), false);
    }
    return found->second;
}

// --- Finishing the frame -------------------------------------------------------------------------------------------

// Where each element ends up on the canvas once its and its parents' visuals and clips apply.
void UiKernel::ComputeVisibleRects() {
    struct Mapping {
        float scale = 1.0f;
        Float2 offset{};
        FloatRect clip{};
        bool clipped = false;
    };
    std::vector<Mapping> mappings(_nodes.size());
    for (std::size_t index = 0; index < _nodes.size(); ++index) {
        UiNode& node = _nodes[index];
        const UiLayoutNode& placed = _layout[index];
        Mapping mapping;
        if (placed.parent >= 0) mapping = mappings[static_cast<std::size_t>(placed.parent)];
        // Around the element's own centre c: p -> c + (p - c) * s + o, then the parents' mapping.
        const Float2 center = placed.rect.Center();
        const float s = node.visual.scale;
        const Float2 own{center.x * (1.0f - s) + node.visual.offset.x, center.y * (1.0f - s) + node.visual.offset.y};
        mapping.offset = {mapping.offset.x + mapping.scale * own.x, mapping.offset.y + mapping.scale * own.y};
        mapping.scale *= s;
        const FloatRect mapped{mapping.offset.x + mapping.scale * placed.rect.x,
                               mapping.offset.y + mapping.scale * placed.rect.y, placed.rect.width * mapping.scale,
                               placed.rect.height * mapping.scale};
        const FloatRect visible = mapping.clipped ? mapped.Intersect(mapping.clip) : mapped;
        if (node.clip) {
            mapping.clip = visible;
            mapping.clipped = true;
        }
        mappings[index] = mapping;
        node.entry->visible = visible;
        node.entry->full = mapped;
        node.entry->size = placed.rect.Size();
        node.entry->focusInside.reset();
        node.state.rect = placed.rect;
        node.state.font = settings.font;
        node.state.enabled = node.enabled;
        node.state.age = node.entry->age;
    }
}

void UiKernel::CollectHits() {
    _hits.clear();
    for (const UiNode& node : _nodes) {
        if (node.interact == UiInteract::None || node.entry->full.Empty()) continue;
        _hits.push_back({node.id, node.entry->visible, node.entry->full, node.layer, node.owner, node.interact,
                         node.capture, node.enabled});
    }
    std::ranges::stable_sort(_hits, {}, &UiHit::layer);
}

// A cursor whose element has gone moves to the nearest one left; a screen with no cursor yet gets one on its first.
void UiKernel::RepairFocus() {
    for (int user = 0; user < UserCount(); ++user) {
        const auto slot = static_cast<std::size_t>(user);
        const std::optional<UiId>& scope = _userInput[slot].scopeLayer;
        if (!scope) continue;
        UiLayerMemory& memory = MemoryOf(*scope);
        std::vector<const UiHit*> items;
        std::vector<FloatRect> rects;
        for (const UiHit& hit : _hits) {
            if (_lastLayers[static_cast<std::size_t>(hit.layer)].id != *scope || !UserMayFocus(hit, user)) continue;
            items.push_back(&hit);
            rects.push_back(hit.full);
        }
        if (memory.hasFocus[slot]) {
            const UiId current = memory.focus[slot];
            const auto still = std::ranges::find_if(items, [&](const UiHit* hit) { return hit->id == current; });
            if (still != items.end()) {
                memory.focusRect[slot] = (*still)->full;
                continue;
            }
            if (const auto nearest = FindNearestUiFocus(rects, memory.focusRect[slot])) {
                MoveFocus(*items[*nearest], user, false);
            } else {
                memory.hasFocus[slot] = false;
            }
        } else if (settings.autoFocus && !items.empty()) {
            MoveFocus(*items.front(), user, false);
        }
    }
}

void UiKernel::SnapshotDrawStates() {
    for (int index = 0; index < static_cast<int>(_nodes.size()); ++index) {
        UiNode& node = Node(index);
        node.state.hovered = Hovered(index);
        node.state.pressed = Pressed(index);
        node.state.focusUsers = 0;
        for (int user = 0; user < UserCount(); ++user) {
            if (!Focused(index, user)) continue;
            const Color4 color = _users[static_cast<std::size_t>(user)].color;
            if (node.state.focusUsers == 0) {
                node.state.cursorColor = color;
            } else if (std::popcount(node.state.focusUsers) == 1) {
                node.state.secondCursorColor = color;
            }
            node.state.focusUsers |= std::uint32_t{1} << user;
        }
        node.state.focused = node.state.focusUsers != 0;
        if (node.state.focused) {
            // Every parent learns where it is, for a scrolling area to follow.
            const FloatRect where = node.entry->full;
            for (int up = index; up >= 0; up = LayoutNode(up).parent) {
                if (!Node(up).entry->focusInside) Node(up).entry->focusInside = where;
            }
        }
    }
}

void UiKernel::ForgetUnseen() {
    if (static_cast<std::size_t>(_seen) != _entries.size()) {
        std::erase_if(_entries, [this](const auto& item) { return item.second.lastFrame != _frame; });
    }
    std::erase_if(_layerMemory, [this](const auto& item) { return item.second.lastFrame != _frame; });
    if (_hot && !std::ranges::any_of(_hits, [this](const UiHit& hit) { return hit.id == *_hot; })) _hot.reset();
}
