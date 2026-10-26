#include "Modules/Localization/Internals/LanguageInfo.h"

LanguageInfo InfoForLanguage(Language language) {
    switch (language) {
    case Language::English: return {"English", false};
    case Language::Turkish: return {"Türkçe", true};
    case Language::Welsh  : return {"Cymraeg", false};
    }
    return {"English", false};
}
