#include "Runtime/ProjectSettingsStore.h"
#include <algorithm>
#include <array>
#include <cstdio>
#include <filesystem>
#include <mutex>
#include <optional>
#include <string>
#include <type_traits>
#include "Platform/Internals/FileStorage.h"
#include "Tuning/TuningLive.h"

namespace {
constexpr std::string_view ValuesFile = "Game/Game.settings.json";
constexpr std::size_t MaxFileBytes = 1024 * 1024;

struct Store {
    std::mutex mutex;
    std::optional<ProjectSettings> compiled;
    bool readFile = false;
    std::optional<ProjectSettings> current;
};

Store& State() {
    static Store store;
    return store;
}

// Writes each field's value into the struct member of the same name.
class Writer {
public:
    explicit Writer(const std::vector<TuningField>& fields)
        : _fields(fields) {}

    template <class T> void Field(std::string_view name, T& member) {
        const auto& value = Find(name);
        if constexpr (std::is_same_v<T, std::string>) {
            if (const auto* text = std::get_if<TuningText>(&value))
                member = text->text;
            else
                member = std::get<TuningPath>(value).path;
        } else {
            member = std::get<T>(value);
        }
    }
    template <class E, std::size_t N>
    void Choice(std::string_view name, E& member, const std::array<std::string_view, N>&) {
        member = static_cast<E>(std::get<TuningChoice>(Find(name)).index);
    }

private:
    const TuningValue& Find(std::string_view name) const {
        const auto found = std::ranges::find(_fields, name, &TuningField::name);
        if (found == _fields.end() || found->values.size() != 1)
            throw TuningFormatError(1, "the schema has no setting \"" + std::string(name) + "\"");
        return found->values.front();
    }
    const std::vector<TuningField>& _fields;
};

std::string Utf8(const std::filesystem::path& path) {
    const auto text = path.generic_u8string();
    return {text.begin(), text.end()};
}

// Development: the file as it is now, or the built values with a warning.
ProjectSettings ReadFile(const ProjectSettings& built) {
    const auto file = TuningRoot() / std::filesystem::path(ValuesFile);
    std::error_code error;
    if (!std::filesystem::is_regular_file(file, error)) return ProjectSettings{};
    try {
        return ReadProjectSettings(ReadBinaryFile(file, MaxFileBytes));
    } catch (const TuningFormatError& failure) {
        std::fprintf(stderr, "%s:%d: project settings not applied: %s; using the built values\n", Utf8(file).c_str(),
                     failure.Line(), failure.what());
    } catch (const std::exception& failure) {
        std::fprintf(stderr, "%s: project settings unreadable: %s; using the built values\n", Utf8(file).c_str(),
                     failure.what());
    }
    return built;
}
} // namespace

void RegisterProjectSettings(ProjectSettings compiled, bool readFile) {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    state.compiled = std::move(compiled);
    state.readFile = readFile;
    state.current.reset();
}

const ProjectSettings& CurrentProjectSettings() {
    auto& state = State();
    const std::scoped_lock lock(state.mutex);
    if (!state.current) {
        const ProjectSettings built = state.compiled.value_or(ProjectSettings{});
        state.current = state.compiled && state.readFile ? ReadFile(built) : built;
    }
    return *state.current;
}

const std::vector<TuningField>& ProjectSettingsSchema() {
    static const std::vector<TuningField> schema = ParseSettingsSchema(ProjectSettings::Schema);
    return schema;
}

ProjectSettings ReadProjectSettings(std::string_view text) {
    const auto values = ParseSettingsValues(text, ProjectSettingsSchema());
    ProjectSettings settings;
    Writer writer(values.fields);
    settings.VisitSettings(writer);
    return settings;
}
