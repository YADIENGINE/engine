#include "GameObjects/ObjectList.h"
#include <cstdio>
#include <exception>
#include <stdexcept>
#include <utility>

ObjectList::ObjectList() = default;
ObjectList::~ObjectList() {
    try {
        Stop();
    } catch (const std::exception& error) {
        std::fprintf(stderr, "ObjectList shutdown failed: %s\n", error.what());
    } catch (...) {
        std::fputs("ObjectList shutdown failed with an unknown exception\n", stderr);
    }
}

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
    // Start/Update keep the dispatch guard active through removal callbacks.
    // Reserve before moving registrations so an allocation failure leaves them intact.
    _entries.reserve(_entries.size() + _pending.size());
    // Stop callbacks may enqueue additions; keep those for the next dispatch.
    for (Entry& entry : _pending)
        _entries.push_back(std::move(entry));
    _pending.clear();
    std::exception_ptr failure;
    const auto remove = [&failure](Entry& entry) {
        if (!entry.removed) return false;
        entry.object->_list = nullptr;
        if (entry.object->_started && !entry.object->_stopped) {
            entry.object->_stopped = true;
            try {
                entry.object->Stop();
            } catch (...) {
                if (!failure) failure = std::current_exception();
            }
        }
        return true;
    };
    std::erase_if(_entries, remove);
    if (failure) std::rethrow_exception(failure);
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
    if (_stopped) return;
    _dispatching = true;
    try {
        Commit();
        StartEnabled();
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}

void ObjectList::Update(float deltaSeconds) {
    if (_dispatching) throw std::logic_error("Cannot dispatch GameObjects recursively");
    if (_stopped) return;
    _dispatching = true;
    try {
        Commit();
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

void ObjectList::Draw(Painter& paint) const {
    if (_dispatching) throw std::logic_error("Cannot dispatch GameObjects recursively");
    if (_stopped) return;
    _dispatching = true;
    try {
        for (const Entry& entry : _entries) {
            if (!entry.removed && entry.object->_enabled && entry.object->_started) entry.object->Draw(paint);
        }
    } catch (...) {
        _dispatching = false;
        throw;
    }
    _dispatching = false;
}

void ObjectList::Stop() {
    if (_dispatching) throw std::logic_error("Cannot dispatch GameObjects recursively");
    if (_stopped) return;
    _dispatching = true;
    _stopped = true;
    std::exception_ptr failure;
    for (auto i = _entries.rbegin(); i != _entries.rend(); ++i) {
        i->object->_list = nullptr;
        if (i->object->_started && !i->object->_stopped) {
            i->object->_stopped = true;
            try {
                i->object->Stop();
            } catch (...) {
                if (!failure) failure = std::current_exception();
            }
        }
    }
    for (Entry& entry : _pending)
        entry.object->_list = nullptr;
    _pending.clear();
    _entries.clear();
    _dispatching = false;
    if (failure) std::rethrow_exception(failure);
}
