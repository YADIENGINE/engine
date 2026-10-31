#include "Modules/Persistence/Internals/EncryptionKey.h"
#include <algorithm>
#include "Modules/Persistence/Persistence.h"
#include "Modules/Persistence/Internals/SensitiveMemory.h"

EncryptionKey::EncryptionKey(std::string bytes) {
    if (bytes.size() != _bytes.size()) {
        WipeKeyMemory(bytes.data(), bytes.size());
        throw SaveError(SaveErrorCode::KeyUnavailable, "Invalid encryption key length");
    }
    std::copy(bytes.begin(), bytes.end(), _bytes.begin());
    WipeKeyMemory(bytes.data(), bytes.size());
}
EncryptionKey::~EncryptionKey() { WipeKeyMemory(_bytes.data(), _bytes.size()); }
