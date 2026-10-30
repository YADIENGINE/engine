#pragma once
#include <memory>
#include <vector>
#include "GameObjects/GameObject.h"

// Ordered lifecycle dispatch. New entries begin on the next dispatch; removed entries stop before destruction.
class ObjectList {
public:
    ObjectList();
    ~ObjectList();
    void Add(GameObject& object);
    GameObject& Add(std::unique_ptr<GameObject> object);
    void Remove(GameObject& object);
    void Clear();
    void Start();
    void Update(float deltaSeconds);
    void Draw() const;
    // Completes cleanup before rethrowing the first Stop callback failure.
    void Stop();

private:
    struct Entry {
        GameObject* object;
        std::unique_ptr<GameObject> owner;
        bool removed = false;
    };
    void Commit();
    void StartEnabled();

    std::vector<Entry> _entries;
    std::vector<Entry> _pending;
    mutable bool _dispatching = false;
    bool _stopped = false;

public:
    ObjectList(const ObjectList&) = delete;
    ObjectList& operator=(const ObjectList&) = delete;
    ObjectList(ObjectList&&) = delete;
    ObjectList& operator=(ObjectList&&) = delete;
};
