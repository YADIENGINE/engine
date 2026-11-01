#pragma once
struct Record {
    Record() = default;
    explicit Record(int value);
    Record(const Record&) = default;
    Record(Record&& other) noexcept {}
    Record(Record const& other, int unused = 0);
    Record& operator=(const Record&) = default;
    Record& operator=(Record&&) noexcept;
    Record& operator=(Record other);
    Record& operator=(int value);
    ~Record() {}
    void Unavailable() = delete;
    bool operator==(const Record&) const = default;
};

template <class T> struct Generic {
    Generic(const Generic<T>& other);
    Generic(Generic<T>&& other);
    Generic& operator=(const Generic<T>& other);
    explicit Generic(const T& value);
    ~Generic();
};

void UnavailableFreeFunction() = delete;
