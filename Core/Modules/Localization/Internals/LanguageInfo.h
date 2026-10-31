#pragma once
#include "Modules/Localization/Localization.h"

// Module-private language presentation policy.
struct LanguageInfo {
    const char* nativeName;
    bool percentFirst;
};

[[nodiscard]] LanguageInfo InfoForLanguage(Language language);
