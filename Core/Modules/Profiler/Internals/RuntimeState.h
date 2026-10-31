#pragma once
#include "Modules/Profiler/Profiler.h"

struct Profiler::Runtime {
    virtual ~Runtime() = default;
    virtual void Poll() = 0;
    [[nodiscard]] virtual bool Supported() const = 0;
    [[nodiscard]] virtual std::size_t Pending() const = 0;
    [[nodiscard]] virtual Metadata MetadataValue() const = 0;
};
