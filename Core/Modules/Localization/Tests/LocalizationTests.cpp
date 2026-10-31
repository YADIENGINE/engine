#include "Modules/Localization/Localization.h"
#include <catch2/catch_test_macros.hpp>

namespace {
enum class SampleText { Title, Hint, Count };
using Row = Localization::Row<SampleText, 3>;
constexpr std::array Languages{Localization::LanguageDefinition{Language::English, false, true},
                               Localization::LanguageDefinition{Language::Turkish, true, true},
                               Localization::LanguageDefinition{Language::Welsh, true, false}};
constexpr Language Fallback = Language::English;
constexpr Language Unknown = static_cast<Language>(999);
constexpr auto FirstGroup = Loc::Texts<SampleText>(Languages, [](auto& text) {
    using enum SampleText;
    text(Hint, "Fallback {count}", "Selected {count}", "");
});
constexpr auto SecondGroup = Loc::Texts<SampleText>(Languages, [](auto& text) {
    using enum SampleText;
    text(Title, "Fallback title", "Selected title", "Optional title");
});
static_assert(FirstGroup.size() == 1);
constexpr std::array Groups{Localization::Group(FirstGroup), Localization::Group(SecondGroup)};
constexpr std::size_t TextCount = static_cast<std::size_t>(SampleText::Count);
static_assert(Localization::RowsValid<TextCount>(Groups, Languages, Fallback));
constexpr auto Table = Localization::BuildTable<TextCount>(Groups, Languages, Fallback);
static_assert(std::string_view(Table[0]) == "Fallback title");
static_assert(std::string_view(Table[3]) == "Fallback {count}");
static_assert(static_cast<std::int64_t>(Language::English) == 0);
static_assert(static_cast<std::int64_t>(Language::Turkish) == 1);
static_assert(static_cast<std::int64_t>(Language::Welsh) == 2);

constexpr std::array Missing{std::span<const Row>(FirstGroup)};
constexpr std::array Duplicate{std::span<const Row>(FirstGroup), std::span<const Row>(FirstGroup)};
constexpr std::array InvalidId{Row{SampleText::Count, {"a", "b", ""}}};
constexpr std::array EmptyRequired{Row{SampleText::Title, {"a", "", ""}}};
constexpr std::array NullOptional{Row{SampleText::Title, {"a", "b", nullptr}}};
constexpr std::array EmptyFallback{Row{SampleText::Title, {"", "b", "c"}}};
static_assert(!Localization::RowsValid<TextCount>(Missing, Languages, Fallback));
static_assert(!Localization::RowsValid<TextCount>(Duplicate, Languages, Fallback));
static_assert(!Localization::RowsValid<1>(std::array{std::span<const Row>(InvalidId)}, Languages, Fallback));
static_assert(!Localization::RowsValid<1>(std::array{std::span<const Row>(EmptyRequired)}, Languages, Fallback));
static_assert(!Localization::RowsValid<1>(std::array{std::span<const Row>(NullOptional)}, Languages, Fallback));
constexpr std::array OptionalFallbackLanguages{Localization::LanguageDefinition{Language::English, false, false},
                                               Localization::LanguageDefinition{Language::Turkish, true, true},
                                               Localization::LanguageDefinition{Language::Welsh, true, false}};
static_assert(!Localization::RowsValid<1>(std::array{std::span<const Row>(EmptyFallback)}, OptionalFallbackLanguages,
                                          Fallback));
constexpr std::array DuplicateLanguages{Localization::LanguageDefinition{Language::Turkish},
                                        Localization::LanguageDefinition{Language::Turkish}};
constexpr std::array DisabledLanguages{Localization::LanguageDefinition{Language::Turkish, false},
                                       Localization::LanguageDefinition{Language::Welsh, false}};
constexpr std::array UnknownLanguages{Localization::LanguageDefinition{Unknown}};
static_assert(!Localization::LanguagesValid(DuplicateLanguages, Language::Turkish));
static_assert(!Localization::LanguagesValid(DisabledLanguages, Language::Turkish));
static_assert(!Localization::LanguagesValid(UnknownLanguages, Unknown));
static_assert(!Localization::LanguagesValid(Languages, Unknown));
static_assert(!Localization::LanguagesValid({}, Fallback));
static_assert(!Localization::RowsValid<0>(Groups, Languages, Fallback));
static_assert(!Localization::RowsValid<TextCount>(Groups, DuplicateLanguages, Language::Turkish));
constexpr std::array WrongColumnLanguages{Languages[0], Languages[1]};
static_assert(!Localization::RowsValid<TextCount>(Groups, WrongColumnLanguages, Fallback));

enum class SampleEntity { First, Second, Count };
constexpr auto EntityBindings = Loc::Bindings<SampleEntity, SampleText>([](auto& bind) {
    using enum SampleText;
    bind(SampleEntity::First, Title, Hint);
    bind(SampleEntity::Second, Hint);
});
constexpr Localization::BindingSet<SampleEntity, SampleText> EntityKeys{EntityBindings};
static_assert(Localization::KeyFor(EntityKeys, SampleEntity::First, TextField::Name) == SampleText::Title);
static_assert(Localization::KeyFor(EntityKeys, SampleEntity::First, TextField::Description) == SampleText::Hint);
static_assert(Loc::NameKey(EntityKeys, SampleEntity::First) == SampleText::Title);
static_assert(Loc::DescKey(EntityKeys, SampleEntity::First) == SampleText::Hint);
constexpr std::array MissingBinding{EntityBindings[0]};
constexpr std::array DuplicateBinding{EntityBindings[0], EntityBindings[0]};
constexpr std::array UnorderedBinding{EntityBindings[1], EntityBindings[0]};
constexpr std::array InvalidNameBinding{Localization::Bind(SampleEntity::First, SampleText::Count), EntityBindings[1]};
constexpr std::array InvalidDescriptionBinding{
    Localization::Bind(SampleEntity::First, SampleText::Title, SampleText::Count), EntityBindings[1]};
constexpr std::array InvalidEntityBinding{EntityBindings[0],
                                          Localization::Bind(SampleEntity::Count, SampleText::Title)};
constexpr std::array<Localization::Binding<SampleEntity, SampleText>, 0> EmptyBindings{};
static_assert(!Localization::BindingsValid(MissingBinding));
static_assert(!Localization::BindingsValid(DuplicateBinding));
static_assert(!Localization::BindingsValid(UnorderedBinding));
static_assert(!Localization::BindingsValid(InvalidNameBinding));
static_assert(!Localization::BindingsValid(InvalidDescriptionBinding));
static_assert(!Localization::BindingsValid(InvalidEntityBinding));
static_assert(!Localization::BindingsValid(EmptyBindings));
} // namespace

TEST_CASE("Localization.Bindings.ResolveFieldsAndRejectMissingData", "[localization]") {
    const Localization catalog(Table, {Languages, Fallback});
    CHECK(std::string_view(catalog.Name(Language::Turkish, EntityKeys, SampleEntity::First)) == "Selected title");
    CHECK(std::string_view(catalog.Desc(Language::Welsh, EntityKeys, SampleEntity::First)) == "Fallback {count}");
    CHECK(catalog.FormatDesc(Language::Turkish, EntityKeys, SampleEntity::First, {{"count", "3"}}) == "Selected 3");
    CHECK_THROWS_AS(catalog.Desc(Language::English, EntityKeys, SampleEntity::Second), std::invalid_argument);
    CHECK(std::string_view(catalog.Lookup(Language::Turkish, EntityKeys, SampleEntity::First, TextField::Name)) ==
          "Selected title");
    CHECK(std::string_view(catalog.Lookup(Language::Welsh, EntityKeys, SampleEntity::First, TextField::Description)) ==
          "Fallback {count}");
    CHECK(catalog.Format(Language::Turkish,
                         Localization::KeyFor(EntityKeys, SampleEntity::First, TextField::Description),
                         {{"count", "3"}}) == "Selected 3");
    CHECK_THROWS_AS(Localization::KeyFor(EntityKeys, SampleEntity::Second, TextField::Description),
                    std::invalid_argument);
    CHECK_THROWS_AS(Localization::KeyFor(EntityKeys, SampleEntity::Count, TextField::Name), std::out_of_range);
    CHECK_THROWS_AS(Localization::KeyFor(EntityKeys, static_cast<SampleEntity>(-1), TextField::Name),
                    std::out_of_range);
    CHECK_THROWS_AS(Localization::KeyFor(EntityKeys, SampleEntity::First, static_cast<TextField>(999)),
                    std::invalid_argument);
    CHECK_THROWS_AS(Localization::Bindings(EntityBindings[0]), std::invalid_argument);
    CHECK_THROWS_AS(Localization::Bindings(EntityBindings[1], EntityBindings[0]), std::invalid_argument);
}

TEST_CASE("Localization.Authoring.RejectsMissingAndExcessBindingRows", "[localization]") {
    CHECK_THROWS_AS(
        (Loc::Bindings<SampleEntity, SampleText>([](auto& bind) { bind(SampleEntity::First, SampleText::Title); })),
        std::invalid_argument);
    CHECK_THROWS_AS((Loc::Bindings<SampleEntity, SampleText>([](auto& bind) {
                        bind(SampleEntity::First, SampleText::Title);
                        bind(SampleEntity::Second, SampleText::Hint);
                        bind(SampleEntity::Count, SampleText::Title);
                    })),
                    std::invalid_argument);
}

TEST_CASE("Localization.Selection.UsesOnlyEnabledLanguagesInAuthoredOrder", "[localization]") {
    const Localization catalog(Table, {Languages, Fallback});
    const auto options = catalog.AvailableLanguages();
    REQUIRE(options.size() == 2);
    CHECK(options[0].id == Language::Turkish);
    CHECK(std::string_view(options[0].nativeName) == "Türkçe");
    CHECK(options[1].id == Language::Welsh);
    CHECK(std::string_view(options[1].nativeName) == "Cymraeg");
    CHECK(catalog.NormalizeLanguage(0) == Language::Turkish);
    CHECK(catalog.NormalizeLanguage(999) == Language::Turkish);
    CHECK(catalog.NormalizeLanguage(1) == Language::Turkish);
    CHECK(catalog.NormalizeLanguage(2) == Language::Welsh);
    CHECK(catalog.StepLanguage(Language::Turkish, 1) == Language::Welsh);
    CHECK(catalog.StepLanguage(Language::Welsh, 1) == Language::Turkish);
    CHECK(catalog.StepLanguage(Language::Turkish, -1) == Language::Welsh);
    CHECK(catalog.StepLanguage(Language::Welsh, -1) == Language::Turkish);
    CHECK(catalog.StepLanguage(Language::English, 0) == Language::Welsh);
    CHECK(catalog.StepLanguage(Unknown, -1) == Language::Welsh);
}

TEST_CASE("Localization.Lookup.AllowsDisabledLanguagesAndFallsBackPerText", "[localization]") {
    const Localization catalog(Table, {Languages, Fallback});
    CHECK(std::string_view(catalog.Lookup(Language::English, SampleText::Title)) == "Fallback title");
    CHECK(std::string_view(catalog.Lookup(Language::Turkish, SampleText::Hint)) == "Selected {count}");
    CHECK(std::string_view(catalog.Lookup(Language::Welsh, SampleText::Title)) == "Optional title");
    CHECK(std::string_view(catalog.Lookup(Language::Welsh, SampleText::Hint)) == "Fallback {count}");
    CHECK(std::string_view(catalog.Lookup(Unknown, SampleText::Title)) == "Fallback title");
}

TEST_CASE("Localization.Catalogues.AreIndependentAndPreserveConfiguredOrder", "[localization]") {
    static constexpr std::array singleLanguage{Localization::LanguageDefinition{Language::English}};
    static constexpr std::array rows{Localization::Row<int, 1>{0, {"Another game"}}};
    static constexpr std::array groups{std::span<const Localization::Row<int, 1>>(rows)};
    static constexpr auto table = Localization::BuildTable<1>(groups, singleLanguage, Language::English);
    const Localization single(table, {singleLanguage, Language::English});
    const Localization other(Table, {Languages, Fallback});
    CHECK(single.NormalizeLanguage(-1) == Language::English);
    CHECK(single.StepLanguage(Language::English, 1) == Language::English);
    CHECK(single.StepLanguage(Language::English, -1) == Language::English);
    CHECK(std::string_view(single.AvailableLanguages().front().nativeName) == "English");
    CHECK(std::string_view(single.Lookup(Language::English, 0)) == "Another game");
    CHECK(std::string_view(other.Lookup(Language::Turkish, SampleText::Title)) == "Selected title");
    constexpr std::array reversedLanguages{Languages[2], Languages[1], Languages[0]};
    constexpr std::array<const char*, 3> reversedTable{"Optional", "Required", "Fallback"};
    const Localization reversed(reversedTable, {reversedLanguages, Fallback});
    CHECK(reversed.AvailableLanguages()[0].id == Language::Welsh);
    CHECK(reversed.AvailableLanguages()[1].id == Language::Turkish);
}

TEST_CASE("Localization.Validation.RejectsInvalidBorrowedData", "[localization]") {
    CHECK_THROWS_AS(Localization(Table, {{}, Fallback}), std::invalid_argument);
    CHECK_THROWS_AS(Localization(Table, {Languages, Unknown}), std::invalid_argument);
    CHECK_THROWS_AS(Localization(Table, {DuplicateLanguages, Language::Turkish}), std::invalid_argument);
    CHECK_THROWS_AS(Localization(Table, {DisabledLanguages, Language::Turkish}), std::invalid_argument);
    CHECK_THROWS_AS(Localization(Table, {UnknownLanguages, Unknown}), std::invalid_argument);
    CHECK_THROWS_AS(Localization({}, {Languages, Fallback}), std::invalid_argument);
    CHECK_THROWS_AS(Localization(std::span(Table).first(1), {Languages, Fallback}), std::invalid_argument);
    constexpr std::array<const char*, 3> nullOptional{"a", "b", nullptr};
    constexpr std::array<const char*, 3> emptyRequired{"a", "", "c"};
    constexpr std::array<const char*, 3> emptyFallback{"", "b", "c"};
    CHECK_THROWS_AS(Localization(nullOptional, {Languages, Fallback}), std::invalid_argument);
    CHECK_THROWS_AS(Localization(emptyRequired, {Languages, Fallback}), std::invalid_argument);
    CHECK_THROWS_AS(Localization(emptyFallback, {OptionalFallbackLanguages, Fallback}), std::invalid_argument);
    CHECK_THROWS_AS(Localization::BuildTable<TextCount>(Missing, Languages, Fallback), std::invalid_argument);
}

TEST_CASE("Localization.Formatting.SubstitutesAndPreservesUnknownPlaceholders", "[localization]") {
    const Localization catalog(Table, {Languages, Fallback});
    CHECK(catalog.Format(Language::Turkish, SampleText::Hint, {{"count", "3"}}) == "Selected 3");
    CHECK(catalog.Format(Language::Welsh, SampleText::Hint, {{"count", "3"}}) == "Fallback 3");
    CHECK(catalog.Format(Language::English, SampleText::Hint, {{"other", "3"}}) == "Fallback {count}");
    CHECK(catalog.Format(Language::English, SampleText::Hint, {{"count", "{count}"}}) == "Fallback {count}");
    CHECK(Localization::FormatShortNumber(2.0f) == "2");
    CHECK(Localization::FormatShortNumber(0.5f) == "0.5");
    CHECK(Localization::FormatShortNumber(1.25f) == "1.25");
    CHECK(Localization::FormatShortNumber(-0.001f) == "0");
    CHECK(Localization::FormatMultiplier(1.54f) == "1.5");
    CHECK(Localization::FormatMultiplier(1.56f) == "1.6");
    CHECK(Localization::FormatSignedPercent(0.25f, Language::English) == "+25%");
    CHECK(Localization::FormatSignedPercent(-0.3f, Language::English) == "-30%");
    CHECK(Localization::FormatSignedPercent(0.25f, Language::Turkish) == "+%25");
    CHECK(Localization::FormatSignedPercent(-0.3f, Language::Turkish) == "-%30");
    CHECK(Localization::FormatSignedPercent(0.25f, Language::Welsh) == "+25%");
}
