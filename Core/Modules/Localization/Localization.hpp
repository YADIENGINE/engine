#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// Persisted values: never reorder or renumber.
enum class Language : std::int64_t { English = 0, Turkish = 1, Welsh = 2 };
enum class TextField { Name, Description };

// Immutable catalogue. Borrows metadata, table and strings; they must outlive this object.
class Localization {
public:
    struct LanguageDefinition {
        Language id;
        bool enabled = true;
        bool required = false;
    };

    struct LanguageOption {
        Language id;
        const char* nativeName;
    };

    struct Settings {
        std::span<const LanguageDefinition> languages;
        Language fallbackLanguage;
    };

    struct Argument {
        std::string_view name;
        std::string value;
    };

    template <class Id, std::size_t LanguageCount> struct Row {
        Id id;
        std::array<const char*, LanguageCount> values;
    };

    template <class Id, class Key> struct Binding {
        Id id;
        Key name;
        std::optional<Key> description;
    };

    // Borrowed view of a validated Bindings() array, which must outlive the view.
    template <class Id, class Key> struct BindingSet {
        std::span<const Binding<Id, Key>> rows;
    };

    template <class Key, class... Values> [[nodiscard]] static constexpr auto Text(Key key, Values... values) {
        static_assert(sizeof...(Values) > 0, "A text must declare at least one language column");
        static_assert((std::is_convertible_v<Values, const char*> && ...), "Translations must be C strings");
        return Row<Key, sizeof...(Values)>{key, {values...}};
    }

    template <class Key, std::size_t LanguageCount, class... Rest>
    [[nodiscard]] static constexpr auto Texts(Row<Key, LanguageCount> first, Rest... rest) {
        static_assert((std::is_same_v<Row<Key, LanguageCount>, Rest> && ...),
                      "All texts in a group must use the same key type and language column count");
        return std::array{first, rest...};
    }

    // Captureless authoring callback. A counting pass determines the exact static array size.
    template <class Key, std::size_t LanguageCount, class Author>
    [[nodiscard]] static constexpr auto Texts(const std::array<LanguageDefinition, LanguageCount>&, Author author) {
        static_assert(std::is_empty_v<Author> && std::is_default_constructible_v<Author>,
                      "Text authoring must use a captureless callback");
        constexpr std::size_t RowCount = [] {
            std::size_t count = 0;
            auto countRow = [&count]<class... Values>(Key, Values...) {
                static_assert(sizeof...(Values) == LanguageCount, "Translation columns must match authored languages");
                ++count;
            };
            Author{}(countRow);
            return count;
        }();
        static_assert(RowCount > 0, "A text group must contain at least one row");
        std::array<Row<Key, LanguageCount>, RowCount> rows{};
        std::size_t index = 0;
        auto text = [&rows, &index](Key key, auto... values) {
            static_assert(sizeof...(values) == LanguageCount, "Translation columns must match authored languages");
            if (index >= rows.size()) throw std::invalid_argument("Text authoring changed its row count");
            rows[index++] = Text(key, values...);
        };
        author(text);
        if (index != rows.size()) throw std::invalid_argument("Text authoring changed its row count");
        return rows;
    }

    template <class Key, std::size_t LanguageCount, std::size_t N>
    [[nodiscard]] static constexpr auto Group(const std::array<Row<Key, LanguageCount>, N>& rows) {
        return std::span<const Row<Key, LanguageCount>>(rows);
    }

    template <class Id, class Key> [[nodiscard]] static constexpr auto Bind(Id id, Key name) {
        return Binding<Id, Key>{id, name, std::nullopt};
    }

    template <class Id, class Key> [[nodiscard]] static constexpr auto Bind(Id id, Key name, Key description) {
        return Binding<Id, Key>{id, name, description};
    }

    template <class Id, class Key, std::size_t N>
    [[nodiscard]] static constexpr bool BindingsValid(const std::array<Binding<Id, Key>, N>& rows) {
        if constexpr (N == 0 || N != static_cast<std::size_t>(Id::Count)) return false;
        for (std::size_t i = 0; i < N; ++i) {
            const auto& row = rows[i];
            if (static_cast<std::size_t>(row.id) != i ||
                static_cast<std::size_t>(row.name) >= static_cast<std::size_t>(Key::Count) ||
                (row.description && static_cast<std::size_t>(*row.description) >= static_cast<std::size_t>(Key::Count)))
                return false;
        }
        return true;
    }

    template <class Id, class Key, class... Rest>
    [[nodiscard]] static constexpr auto Bindings(Binding<Id, Key> first, Rest... rest) {
        static_assert((std::is_same_v<Binding<Id, Key>, Rest> && ...),
                      "All bindings must use the same entity and text key types");
        const std::array rows{first, rest...};
        if (!BindingsValid(rows)) throw std::invalid_argument("Bindings must cover every ID in order with valid keys");
        return rows;
    }

    template <class Id, class Key, class Author> [[nodiscard]] static constexpr auto Bindings(Author author) {
        std::array<Binding<Id, Key>, static_cast<std::size_t>(Id::Count)> rows{};
        std::size_t index = 0;
        auto bind = [&rows, &index](Id id, Key name, std::optional<Key> description = std::nullopt) {
            if (index >= rows.size()) throw std::invalid_argument("Too many localization bindings");
            rows[index++] = {id, name, description};
        };
        author(bind);
        if (index != rows.size() || !BindingsValid(rows))
            throw std::invalid_argument("Bindings must cover every ID in order with valid keys");
        return rows;
    }

    template <class Id, class Key>
    [[nodiscard]] static constexpr Key KeyFor(BindingSet<Id, Key> bindings, Id id, TextField field) {
        const auto index = static_cast<std::size_t>(id);
        if (index >= bindings.rows.size() || bindings.rows[index].id != id)
            throw std::out_of_range("Invalid localization binding ID");
        const auto& row = bindings.rows[index];
        switch (field) {
        case TextField::Name: return row.name;
        case TextField::Description:
            if (!row.description) throw std::invalid_argument("Localization binding has no description");
            return *row.description;
        }
        throw std::invalid_argument("Invalid localization text field");
    }

    template <class Id, class Key> [[nodiscard]] static constexpr Key NameKey(BindingSet<Id, Key> bindings, Id id) {
        return KeyFor(bindings, id, TextField::Name);
    }

    template <class Id, class Key> [[nodiscard]] static constexpr Key DescKey(BindingSet<Id, Key> bindings, Id id) {
        return KeyFor(bindings, id, TextField::Description);
    }

    Localization(std::span<const char* const> translations, const Settings& settings);
    [[nodiscard]] Language NormalizeLanguage(std::int64_t storedId) const;
    [[nodiscard]] Language StepLanguage(Language language, int direction) const;
    [[nodiscard]] std::span<const LanguageOption> AvailableLanguages() const;

    template <class Id> [[nodiscard]] const char* Lookup(Language language, Id key) const {
        return LookupIndex(language, static_cast<std::size_t>(key));
    }

    template <class Id, class Key>
    [[nodiscard]] const char* Lookup(Language language, BindingSet<Id, Key> bindings, Id id, TextField field) const {
        return Lookup(language, KeyFor(bindings, id, field));
    }

    template <class Id, class Key>
    [[nodiscard]] const char* Name(Language language, BindingSet<Id, Key> bindings, Id id) const {
        return Lookup(language, bindings, id, TextField::Name);
    }

    template <class Id, class Key>
    [[nodiscard]] const char* Desc(Language language, BindingSet<Id, Key> bindings, Id id) const {
        return Lookup(language, bindings, id, TextField::Description);
    }

    template <class Id>
    [[nodiscard]] std::string Format(Language language, Id key, std::initializer_list<Argument> arguments) const {
        return FormatIndex(language, static_cast<std::size_t>(key), arguments);
    }

    template <class Id, class Key>
    [[nodiscard]] std::string FormatDesc(Language language, BindingSet<Id, Key> bindings, Id id,
                                         std::initializer_list<Argument> arguments) const {
        return Format(language, KeyFor(bindings, id, TextField::Description), arguments);
    }

    [[nodiscard]] static std::string FormatShortNumber(float value);
    [[nodiscard]] static std::string FormatMultiplier(float value);
    [[nodiscard]] static std::string FormatSignedPercent(float fraction, Language language);

    [[nodiscard]] static constexpr bool LanguagesValid(std::span<const LanguageDefinition> languages,
                                                       Language fallbackLanguage) {
        bool enabled = false;
        bool fallback = false;
        for (std::size_t i = 0; i < languages.size(); ++i) {
            if (!KnownLanguage(languages[i].id)) return false;
            enabled = enabled || languages[i].enabled;
            fallback = fallback || languages[i].id == fallbackLanguage;
            for (std::size_t j = 0; j < i; ++j) {
                if (languages[i].id == languages[j].id) return false;
            }
        }
        return enabled && fallback;
    }

    template <std::size_t TextCount, class Id, std::size_t LanguageCount, std::size_t GroupCount>
    [[nodiscard]] static constexpr bool
    RowsValid(const std::array<std::span<const Row<Id, LanguageCount>>, GroupCount>& groups,
              std::span<const LanguageDefinition> languages, Language fallbackLanguage) {
        if constexpr (TextCount == 0) {
            return false;
        } else {
            if (languages.size() != LanguageCount || !LanguagesValid(languages, fallbackLanguage)) return false;
            std::array<bool, TextCount> seen{};
            for (const auto group : groups) {
                for (const auto& row : group) {
                    const auto index = static_cast<std::size_t>(row.id);
                    if (index >= TextCount || seen[index]) return false;
                    seen[index] = true;
                    for (std::size_t column = 0; column < LanguageCount; ++column) {
                        const char* value = row.values[column];
                        if (!value ||
                            ((languages[column].required || languages[column].id == fallbackLanguage) && !*value))
                            return false;
                    }
                }
            }
            for (const bool present : seen) {
                if (!present) return false;
            }
            return true;
        }
    }

    template <std::size_t TextCount, class Id, std::size_t LanguageCount, std::size_t GroupCount>
    [[nodiscard]] static constexpr std::array<const char*, TextCount * LanguageCount>
    BuildTable(const std::array<std::span<const Row<Id, LanguageCount>>, GroupCount>& groups,
               std::span<const LanguageDefinition> languages, Language fallbackLanguage) {
        if (!RowsValid<TextCount>(groups, languages, fallbackLanguage)) {
            throw std::invalid_argument("Invalid translation rows or language configuration");
        }
        std::array<const char*, TextCount * LanguageCount> table{};
        for (const auto group : groups) {
            for (const auto& row : group) {
                for (std::size_t column = 0; column < LanguageCount; ++column) {
                    table[static_cast<std::size_t>(row.id) * LanguageCount + column] = row.values[column];
                }
            }
        }
        return table;
    }

private:
    [[nodiscard]] static constexpr bool KnownLanguage(Language language) {
        switch (language) {
        case Language::English:
        case Language::Turkish:
        case Language::Welsh  : return true;
        }
        return false;
    }
    [[nodiscard]] std::string FormatIndex(Language language, std::size_t key,
                                          std::initializer_list<Argument> arguments) const;
    [[nodiscard]] std::size_t LanguageIndex(Language language) const;
    [[nodiscard]] const char* LookupIndex(Language language, std::size_t key) const;
    std::span<const char* const> _translations;
    Settings _settings;
    std::size_t _fallbackIndex;
    std::vector<LanguageOption> _availableLanguages;

public:
    Localization(const Localization&) = delete;
    Localization& operator=(const Localization&) = delete;
    Localization(Localization&&) = delete;
    Localization& operator=(Localization&&) = delete;
};

using Loc = Localization;
