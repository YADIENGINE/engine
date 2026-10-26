#include "Runtime/ObjectList.h"
#include <stdexcept>
#include <utility>

ObjectList::ObjectList() = default;
ObjectList::~ObjectList() { Stop(); }

void ObjectList::Add(GameObject& object) {
    if (_stopped) throw std::logic_error("Cannot register objects after shutdown");
    if (object._stopped) throw std::logic_error("Cannot register an object after its Stop callback");
    if (object._list) throw std::logic_error("GameObject is already registered");
    _pending.push_back({&object, {}, false});
    object._list = this;
}

GameObject& ObjectList::Add(std::unique_ptr<GameObject> object) {
    if (!object) throw std::invalid_argument("Cannot register an empty GameObject");
    GameObject& result = *object;
    Add(result);
    _pending.back().owner = std::move(object);
    return result;
}

void ObjectList::Remove(GameObject& object) {
    for (auto* entries : {&_entries, &_pending}) {
        for (Entry& entry : *entries) {
            if (entry.object == &object) entry.removed = true;
        }
    }
}

void ObjectList::Clear() {
    for (auto* entries : {&_entries, &_pending}) {
        for (Entry& entry : *entries)
            entry.removed = true;
    }
}

void ObjectList::Commit() {
    if (_dispatching || _stopped) return;
    // Stop callbacks may enqueue additions; keep those for the next dispatch.
    auto incoming = std::move(_pending);
    _pending.clear();
    for (Entry& entry : incoming)
        _entries.push_back(std::move(entry));
    const auto remove = [](Entry& entry) {
        if (!entry.removed) return false;
        entry.object->_list = nullptr;
        if (entry.object->_started && !entry.object->_stopped) {
            entry.object->_stopped = true;
            entry.object->Stop();
        }
        return true;
    };
    std::erase_if(_entries, remove);
}

void ObjectList::StartEnabled() {
    for (Entry& entry : _entries) {
        GameObject& object = *entry.object;
        if (!entry.removed && object._enabled && !object._started) {
            object._started = true;
            object.Start();
        }
    }
}

void ObjectList::Start() {
    if (_dispatching) throw std::logic_error("Cannot dispatch GameObjects recursively");
    Commit();
    _dispatching = true;
    try {
        StartEnabled();
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}

void ObjectList::Update(float deltaSeconds) {
    if (_dispatching) throw std::logic_error("Cannot dispatch GameObjects recursively");
    Commit();
    if (_stopped) return;
    _dispatching = true;
    try {
        StartEnabled();
        if (deltaSeconds > 0.0f) {
            for (Entry& entry : _entries) {
                if (!entry.removed && entry.object->_enabled && entry.object->_started) {
                    entry.object->Update(deltaSeconds);
                }
            }
        }
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}

void ObjectList::Draw() const {
    if (_stopped) return;
    for (const Entry& entry : _entries) {
        if (!entry.removed && entry.object->_enabled && entry.object->_started) entry.object->Draw();
    }
}

void ObjectList::Stop() {
    if (_stopped) return;
    _stopped = true;
    for (auto i = _entries.rbegin(); i != _entries.rend(); ++i) {
        i->object->_list = nullptr;
        if (i->object->_started && !i->object->_stopped) {
            i->object->_stopped = true;
            i->object->Stop();
        }
    }
    for (Entry& entry : _pending)
        entry.object->_list = nullptr;
    _pending.clear();
    _entries.clear();
}
