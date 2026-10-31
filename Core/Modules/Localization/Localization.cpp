#include "Modules/Localization/Localization.h"
#include <cassert>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include "Modules/Localization/Internals/LanguageInfo.h"

namespace {
std::string FormatPattern(std::string_view pattern, std::initializer_list<Localization::Argument> arguments) {
    std::string result(pattern);
    for (const auto& argument : arguments) {
        const std::string marker = "{" + std::string(argument.name) + "}";
        std::size_t position = 0;
        while ((position = result.find(marker, position)) != std::string::npos) {
            result.replace(position, marker.size(), argument.value);
            position += argument.value.size();
        }
    }
    return result;
}
} // namespace

Localization::Localization(std::span<const char* const> translations, const Settings& settings)
    : _translations(translations)
    , _settings(settings)
    , _fallbackIndex(LanguageIndex(settings.fallbackLanguage)) {
    if (!LanguagesValid(settings.languages, settings.fallbackLanguage) || translations.empty() ||
        translations.size() % settings.languages.size() != 0) {
        throw std::invalid_argument("Invalid translation catalogue configuration or table dimensions");
    }
    for (std::size_t i = 0; i < translations.size(); ++i) {
        const std::size_t column = i % settings.languages.size();
        const char* value = translations[i];
        if (!value || ((settings.languages[column].required || column == _fallbackIndex) && !*value)) {
            throw std::invalid_argument("Missing required catalogue translation");
        }
    }
    for (const auto& language : settings.languages) {
        if (language.enabled) _availableLanguages.push_back({language.id, InfoForLanguage(language.id).nativeName});
    }
}

std::size_t Localization::LanguageIndex(Language language) const {
    for (std::size_t i = 0; i < _settings.languages.size(); ++i) {
        if (_settings.languages[i].id == language) return i;
    }
    return _settings.languages.size();
}

Language Localization::NormalizeLanguage(std::int64_t storedId) const {
    for (const auto& option : _availableLanguages) {
        if (static_cast<std::int64_t>(option.id) == storedId) return option.id;
    }
    return _availableLanguages.front().id;
}

Language Localization::StepLanguage(Language language, int direction) const {
    const Language normalized = NormalizeLanguage(static_cast<std::int64_t>(language));
    const std::size_t count = _availableLanguages.size();
    std::size_t index = 0;
    while (_availableLanguages[index].id != normalized)
        ++index;
    index = direction < 0 ? (index == 0 ? count - 1 : index - 1) : (index + 1) % count;
    return _availableLanguages[index].id;
}

std::span<const Localization::LanguageOption> Localization::AvailableLanguages() const { return _availableLanguages; }

const char* Localization::LookupIndex(Language language, std::size_t key) const {
    const std::size_t count = _settings.languages.size();
    assert(key < _translations.size() / count && "Invalid catalogue text ID");
    if (key >= _translations.size() / count) return "";
    const std::size_t index = LanguageIndex(language);
    const std::size_t row = key * count;
    const char* text = _translations[row + (index < count ? index : _fallbackIndex)];
    return *text ? text : _translations[row + _fallbackIndex];
}

std::string Localization::FormatIndex(Language language, std::size_t key,
                                      std::initializer_list<Argument> arguments) const {
    return FormatPattern(LookupIndex(language, key), arguments);
}

std::string Localization::FormatShortNumber(float value) {
    std::array<char, 32> buffer{};
    std::snprintf(buffer.data(), buffer.size(), "%.2f", static_cast<double>(value));
    std::string result(buffer.data());
    result.erase(result.find_last_not_of('0') + 1);
    if (result.back() == '.') result.pop_back();
    return result == "-0" ? "0" : result;
}

std::string Localization::FormatSignedPercent(float fraction, Language language) {
    const long percent = std::lround(fraction * 100.0f);
    const std::string sign = percent >= 0 ? "+" : "-";
    const std::string number = std::to_string(std::abs(percent));
    return InfoForLanguage(language).percentFirst ? sign + "%" + number : sign + number + "%";
}

std::string Localization::FormatMultiplier(float value) {
    const long tenths = std::lround(value * 10.0f);
    return std::to_string(tenths / 10) + "." + std::to_string(tenths % 10);
}
