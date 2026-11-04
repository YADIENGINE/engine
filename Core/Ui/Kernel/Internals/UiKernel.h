#pragma once
#include <memory>
#include <optional>
#include <span>
#include <string_view>
#include <typeindex>
#include <unordered_map>
#include <vector>
#include "Input/Input.h"
#include "Ui/Kernel/Internals/UiFocusSearch.h"
#include "Ui/Kernel/Internals/UiTextArena.h"
#include "Ui/Kernel/UiKeys.h"
#include "Ui/Kernel/UiLooks.h"
#include "Ui/Kernel/UiTypes.h"
#include "Ui/Layout/Internals/UiLayoutSolver.h"

struct UiLayerSpec;

// The UI's working parts behind Ui, UiElement and UiLayerScope. Two kinds of data live here: what stays between frames
// (UiEntry, UiLayerMemory, what the last frame left to click on) and what is rebuilt every frame (UiNode).
class UiKernel {
public:
    explicit UiKernel(const UiSettings& settings);

    void SetUsers(std::vector<UiKeys> users);
    void Input(const DeviceInput& input);
    void CancelInput();
    void Begin(float seconds);
    void End();
    void Draw(Painter& paint) const;

    [[nodiscard]] int OpenElement(std::optional<UiId> id, const UiBox& box, const UiTraits& traits);
    void CloseElement(int node) noexcept;
    [[nodiscard]] bool OpenLayer(UiId id, bool open, const UiLayerSpec& spec, float& progress, int& root,
                                 int& savedCursor, int& savedLayer);
    void CloseLayer(int savedCursor, int savedLayer) noexcept;
    [[nodiscard]] UiAction Back();
    [[nodiscard]] int PageStep() const;
    [[nodiscard]] std::optional<Float2> Pointer() const { return _pointer; }
    [[nodiscard]] float Wheel() const { return _wheel; }
    [[nodiscard]] bool CursorMovedByKeys() const { return _keysMovedFrame != 0 && _keysMovedFrame + 1 == _frame; }

    [[nodiscard]] UiId IdOf(int node) const { return Node(node).id; }
    [[nodiscard]] bool Hovered(int node) const;
    [[nodiscard]] bool Pressed(int node) const;
    [[nodiscard]] bool Held(int node) const;
    [[nodiscard]] bool PointerInside(int node) const;
    [[nodiscard]] bool Focused(int node, int user) const;
    [[nodiscard]] bool Enabled(int node) const { return Node(node).enabled; }
    [[nodiscard]] UiAction Activated(int node) const;
    [[nodiscard]] UiStep Step(int node) const;
    [[nodiscard]] bool Denied(int node) const;
    void ReportChange(int node, int user, UiSound sound);
    [[nodiscard]] FloatRect LastRect(int node) const;
    [[nodiscard]] Float2 LastSize(int node) const;
    [[nodiscard]] std::optional<FloatRect> FocusInside(int node) const;
    [[nodiscard]] float Age(int node) const;
    void TakeFocus(int node, int user);
    void SetDraw(int node, UiDrawFunction draw) { Node(node).draw = std::move(draw); }
    void SetVisual(int node, const UiVisual& visual) { Node(node).visual = visual; }
    [[nodiscard]] void* FindState(int node, std::type_index type) const;
    void* AddState(int node, std::type_index type, std::shared_ptr<void> state);

    [[nodiscard]] std::string_view CopyText(std::string_view text) { return _text.Copy(text); }
    [[nodiscard]] const FontFace& Font() const;
    [[nodiscard]] float Delta() const { return _delta; }
    [[nodiscard]] std::span<const UiEvent> Events() const { return _events; }
    [[nodiscard]] int UserCount() const { return static_cast<int>(_users.size()); }

    UiSettings settings;
    UiLooks looks;
    UiStats stats;

private:
    struct StateSlot {
        std::type_index type;
        std::shared_ptr<void> value;
    };
    // An element's memory between frames, found by its id.
    struct UiEntry {
        std::uint64_t lastFrame = 0;
        float age = 0.0f;
        FloatRect visible{}; // On the canvas, after clips and visuals
        FloatRect full{};    // The same without clips: where a cursor can still go
        Float2 size{};
        std::optional<FloatRect> focusInside;
        std::vector<StateSlot> states;
    };
    // One element of the frame being built.
    struct UiNode {
        UiId id{};
        UiEntry* entry = nullptr;
        int layer = 0;
        int owner = UiAnyUser;
        UiInteract interact = UiInteract::None;
        UiCapture capture = UiCapture::None;
        bool enabled = true;
        UiSound sound = UiSound::Auto;
        bool clip = false;
        UiVisual visual{};
        UiMeasure measure{};
        UiDrawFunction draw{};
        UiDrawState state{};
        int children = 0;
    };
    // A layer's memory between frames.
    struct UiLayerMemory {
        std::uint64_t lastFrame = 0;
        float progress = 0.0f;
        bool wasOpen = false;
        std::vector<UiId> focus;          // Per player
        std::vector<FloatRect> focusRect; // Where each cursor was, in case its element goes away
        std::vector<bool> hasFocus;
    };
    // A layer of the frame being built, and afterwards of the last frame.
    struct UiLayerRecord {
        UiId id{};
        int root = -1;
        bool modal = false;
        bool closing = false;
    };
    // What the last frame left to click on or move to, in draw order.
    struct UiHit {
        UiId id{};
        FloatRect visible{};
        FloatRect full{};
        int layer = 0;
        int owner = UiAnyUser;
        UiInteract interact = UiInteract::None;
        UiCapture capture = UiCapture::None;
        bool enabled = true;
        UiSound sound = UiSound::Auto;
    };
    struct UiUserInput {
        std::optional<UiMove> held;
        float heldSeconds = 0.0f;
        float nextRepeat = 0.0f;
        std::optional<UiId> scopeLayer; // The layer this player's cursor is in
        bool confirmDown = false;
    };
    struct UiIdHash {
        std::size_t operator()(UiId id) const { return static_cast<std::size_t>(id.Value()); }
    };

    [[nodiscard]] UiNode& Node(int index) { return _nodes[static_cast<std::size_t>(index)]; }
    [[nodiscard]] const UiNode& Node(int index) const { return _nodes[static_cast<std::size_t>(index)]; }
    [[nodiscard]] UiLayoutNode& LayoutNode(int index) { return _layout[static_cast<std::size_t>(index)]; }
    [[nodiscard]] const UiLayoutNode& LayoutNode(int index) const { return _layout[static_cast<std::size_t>(index)]; }
    [[nodiscard]] bool ClosingLayer(int layer) const { return _layers[static_cast<std::size_t>(layer)].closing; }
    void CheckBuilding(const char* call) const;
    void Report(const char* message);

    // Reading the input against what the last frame showed.
    void ResolveInput();
    void ResolvePointer(const DeviceInput& input);
    void ResolveKeys(const DeviceInput& input);
    [[nodiscard]] std::optional<UiMove> PressedMove(const UiKeys& keys, const DeviceInput& input, UiUserInput& state);
    [[nodiscard]] std::size_t FirstInteractiveLayer() const;
    [[nodiscard]] const UiHit* HitAt(Float2 point) const;
    [[nodiscard]] bool PointerMayUse(const UiHit& hit) const;
    [[nodiscard]] bool UserMayFocus(const UiHit& hit, int user) const;
    [[nodiscard]] std::optional<std::size_t> ScopeLayerOf(int user) const;
    void UpdateScopes();
    void MoveFocus(const UiHit& hit, int user, bool announce);
    void MoveFocusBy(int user, UiMove move);
    void Activate(const UiHit& hit, int user, bool byPointer);
    [[nodiscard]] const UiHit* FocusedHit(int user) const;
    [[nodiscard]] bool DescribingTopLayer() const;
    void Emit(UiEventKind kind, UiId id, int user, const FloatRect& rect, UiSound sound = UiSound::Auto);
    [[nodiscard]] UiLayerMemory& MemoryOf(UiId layer);
    void SizeMemory(UiLayerMemory& memory) const;

    // Finishing the frame.
    void ComputeVisibleRects();
    void CollectHits();
    void RepairFocus();
    void SnapshotDrawStates();
    void ForgetUnseen();
    void DrawNode(Painter& paint, int index) const;

    static constexpr int MaxUsers = 32;

    std::vector<UiKeys> _users;
    std::vector<UiUserInput> _userInput;
    std::optional<DeviceInput> _input;

    // Between frames.
    std::unordered_map<UiId, UiEntry, UiIdHash> _entries;
    std::unordered_map<UiId, UiLayerMemory, UiIdHash> _layerMemory;
    std::vector<UiHit> _hits;
    std::vector<UiLayerRecord> _lastLayers;
    std::optional<UiId> _hot;
    std::optional<UiId> _pressed;
    std::optional<Float2> _pointer;

    // This frame.
    std::vector<UiNode> _nodes;
    std::vector<UiLayoutNode> _layout;
    std::vector<UiLayerRecord> _layers;
    std::vector<UiEvent> _events;
    struct Outcome {
        UiId id{};
        int user = 0;
        bool byPointer = false;
    };
    struct StepOutcome {
        UiId id{};
        UiStep step{};
    };
    std::vector<Outcome> _activated;
    std::vector<StepOutcome> _steps;
    std::vector<UiId> _denied;
    std::vector<int> _backUsers;
    bool _backAnswered = false;
    UiTextArena _text;
    std::uint64_t _frame = 0;
    float _delta = 0.0f;
    float _wheel = 0.0f;
    int _pageStep = 0;
    std::uint64_t _keysMovedFrame = 0;
    int _cursor = -1;
    int _cursorLayer = 0;
    int _seen = 0;
    bool _building = false;
    bool _built = false;
    const char* _scopeError = nullptr;
};
