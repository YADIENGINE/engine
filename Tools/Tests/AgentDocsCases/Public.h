#pragma once
// A small public fixture. More detail follows.
class Widget {
public:
    // Construct a widget.
    explicit Widget(int count = 2) : _count(count) {}
    // Return its count. No mutation occurs.
    [[nodiscard]] int Count() const { return _count; }
    // Set its count.
    void Set(int count);
    static constexpr int Limit = 10;
    struct Result { int value = 0; };
protected:
    void Hook();
private:
    int _count;
    class Hidden { public: void Secret(); };
};
// A free function.
template <class T> T Identity(T value) { return value; }
// A mode.
enum class Mode { First, Second };
