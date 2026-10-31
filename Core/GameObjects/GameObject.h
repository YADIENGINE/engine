#pragma once
class ObjectList;
class Painter;

// Lifecycle-only object. ObjectList dispatches callbacks; registration and ownership are separate.
class GameObject {
public:
    virtual ~GameObject();
    void SetEnabled(bool enabled) { _enabled = enabled; }
    [[nodiscard]] bool Enabled() const { return _enabled; }

protected:
    GameObject();
    virtual void Start();
    virtual void Update(float deltaSeconds);
    // Draws the object for this frame.
    virtual void Draw(Painter& paint) const;
    virtual void Stop();

private:
    friend class ObjectList;
    bool _enabled = true;
    bool _started = false;
    bool _stopped = false;
    ObjectList* _list = nullptr; // One non-owning lifecycle registration at a time.

public:
    GameObject(const GameObject&) = delete;
    GameObject& operator=(const GameObject&) = delete;
    GameObject(GameObject&&) = delete;
    GameObject& operator=(GameObject&&) = delete;
};
