#include <catch2/catch_test_macros.hpp>
#include <limits>
#include "Tests/Support/TempDirectory.h"
#include "Modules/Persistence/Persistence.h"

TEST_CASE("Persistence.Values.GenericRoundTripCopiesNullAndLiteralKeys", "[persistence]") {
    TempDirectory temp;
    using D = Persistence::Document;
    Persistence store(temp.Path() / "generic.json");
    CHECK_FALSE(std::filesystem::exists(store.Path()));
    CHECK_FALSE(store.Get("missing"));
    store.Set("null", nullptr);
    REQUIRE(store.Get("null"));
    CHECK(store.Get("null")->IsNull());
    store.Set("profile", D::Object{{"name", "Türkçe"}, {"tags", D::Array{"one", true, 1.25}}});
    store.Set("nested.dot", std::numeric_limits<std::uint64_t>::max());
    CHECK_FALSE(store.Contains("nested"));
    CHECK_FALSE(std::filesystem::exists(store.Path()));
    auto copy = store.Get("profile");
    REQUIRE(copy);
    (*copy)["name"] = "changed";
    CHECK(store.Get("profile")->At("name").AsString() == "Türkçe");
    CHECK_THROWS_AS(store.GetInt("profile"), SaveError);
    store.Save();
    Persistence reopened(store.Path());
    REQUIRE(reopened.Load() == SaveLoadResult::Loaded);
    CHECK(reopened.Get("profile") == store.Get("profile"));
    CHECK(reopened.Get("nested.dot")->AsUInt64() == std::numeric_limits<std::uint64_t>::max());
    CHECK(reopened.Get("null")->IsNull());
    reopened.Set("profile", *copy);
    CHECK(reopened.Get("profile")->At("name").AsString() == "changed");
    reopened.Erase("profile");
    CHECK_FALSE(reopened.Contains("profile"));
}

TEST_CASE("Persistence.Recovery.CallerSchemaRejectsSemanticallyInvalidSnapshots", "[persistence][recovery]") {
    TempDirectory temp;
    SaveSettings settings{.keepBackup = true};
    settings.acceptsValues = [](const JsonIO::Document::Object& values) {
        return values.at("status").AsString() == "valid";
    };
    Persistence store(temp.Path() / "validated.json", settings);
    store.Set("status", "valid");
    store.Save();
    JsonIO::Write(store.Path(), JsonIO::Document::Object{{"version", 2},
                                                         {"values", JsonIO::Document::Object{{"status", "invalid"}}}});
    REQUIRE(store.Load() == SaveLoadResult::Recovered);
    CHECK(store.Get("status")->AsString() == "valid");
    store.Set("status", "invalid");
    CHECK_THROWS_AS(store.Save(), SaveError);
    Persistence reopened(store.Path(), settings);
    CHECK(reopened.Load() == SaveLoadResult::Loaded);
    CHECK(reopened.Get("status")->AsString() == "valid");
}
