#include "Modules/Persistence/Internals/SensitiveMemory.h"

void WipeKeyMemory(void* memory, std::size_t size) noexcept {
    auto* bytes = static_cast<volatile unsigned char*>(memory);
    while (size-- != 0)
        *bytes++ = 0;
}
