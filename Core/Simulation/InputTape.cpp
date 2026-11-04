#include "Simulation/InputTape.h"
#include <algorithm>
#include <charconv>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <string_view>
#include <utility>
#include "Simulation/StateHash.h"

namespace {
constexpr int TapeFormat = 1;
constexpr std::size_t HashDigits = 16;
constexpr JsonIO::Limits TapeLimits{.maxBytes = std::size_t{1} << 30U, .maxDepth = 16};

#ifdef NDEBUG
constexpr const char* BuildConfiguration = "Release";
#else
constexpr const char* BuildConfiguration = "Debug";
#endif

JsonIO::Document Names(std::uint32_t bits, const std::vector<std::string>& names) {
    auto list = JsonIO::Document::MakeArray();
    for (std::size_t i = 0; i < names.size(); ++i)
        if ((bits >> i) & 1U) list.Append(names[i]);
    return list;
}
JsonIO::Document Strings(const std::vector<std::string>& values) {
    auto list = JsonIO::Document::MakeArray();
    for (const auto& value : values)
        list.Append(value);
    return list;
}
std::vector<std::string> ReadStrings(const JsonIO::Document& list) {
    std::vector<std::string> values;
    for (const auto& item : list.Elements())
        values.push_back(item.AsString());
    return values;
}
std::size_t IndexOf(const std::vector<std::string>& names, const std::string& name) {
    const auto found = std::ranges::find(names, name);
    if (found == names.end()) throw std::runtime_error("Tape names an undeclared action: " + name);
    return static_cast<std::size_t>(found - names.begin());
}
std::uint32_t ReadBits(const JsonIO::Document& list, const std::vector<std::string>& names) {
    std::uint32_t bits = 0;
    for (const auto& item : list.Elements())
        bits |= std::uint32_t{1} << IndexOf(names, item.AsString());
    return bits;
}
const JsonIO::Document* Find(const JsonIO::Document& object, std::string_view key) {
    const auto& members = object.Members();
    const auto found = members.find(key);
    return found == members.end() ? nullptr : &found->second;
}
int IntOf(const JsonIO::Document& value, std::string_view what) {
    const auto number = value.AsInt64();
    if (number < std::numeric_limits<int>::min() || number > std::numeric_limits<int>::max())
        throw std::runtime_error("Tape value out of range: " + std::string(what));
    return static_cast<int>(number);
}
std::string EncodeHashes(const std::vector<std::uint64_t>& hashes) {
    std::string text;
    text.reserve(hashes.size() * HashDigits);
    for (const auto hash : hashes)
        text += FormatStateHash(hash);
    return text;
}
std::vector<std::uint64_t> DecodeHashes(const std::string& text) {
    if (text.size() % HashDigits != 0) throw std::runtime_error("Tape hashes have a wrong length");
    std::vector<std::uint64_t> hashes;
    hashes.reserve(text.size() / HashDigits);
    for (std::size_t i = 0; i < text.size(); i += HashDigits) {
        std::uint64_t value = 0;
        const char* first = text.data() + i;
        const auto [end, error] = std::from_chars(first, first + HashDigits, value, 16);
        if (error != std::errc{} || end != first + HashDigits) throw std::runtime_error("Tape hashes are not hex");
        hashes.push_back(value);
    }
    return hashes;
}
std::uint64_t TuningHash(const std::string& text) {
    const auto hashes = DecodeHashes(text);
    if (hashes.size() != 1) throw std::runtime_error("A tape's tuning hash has a wrong length");
    return hashes.front();
}
} // namespace

InputTape::InputTape(const SimulationSettings& settings, SimulationStart startState)
    : simulation(settings.id)
    , simulationVersion(settings.version)
    , engineVersion(ENGINE_VERSION)
    , configuration(BuildConfiguration)
    , stepsPerSecond(settings.stepsPerSecond)
    , buttons(settings.buttons)
    , axes(settings.axes)
    , start(std::move(startState)) {}

void InputTape::Truncate(std::uint64_t step) {
    if (step >= steps) return;
    steps = step;
    std::erase_if(changes, [step](const Change& change) { return change.at >= step; });
    std::erase_if(commands, [step](const TimedCommand& command) { return command.at >= step; });
    const std::uint64_t kept = step / static_cast<std::uint64_t>(hashEvery) + 1U;
    if (hashes.size() > kept) hashes.resize(static_cast<std::size_t>(kept));
}
std::string InputTape::Incompatibility(const SimulationSettings& settings) const {
    if (simulation != settings.id) return "the tape is for simulation '" + simulation + "', not '" + settings.id + "'";
    if (stepsPerSecond != settings.stepsPerSecond) return "the tape has another step rate";
    if (buttons != settings.buttons || axes != settings.axes) return "the tape has another input schema";
    if (start.players < 1 || start.players > settings.maxPlayers) return "the tape has too many players";
    return {};
}
bool InputTape::SameVersions(const SimulationSettings& settings) const {
    return simulationVersion == settings.version && engineVersion == ENGINE_VERSION;
}

JsonIO::Document InputTape::ToDocument() const {
    auto document = JsonIO::Document::MakeObject();
    document["format"] = TapeFormat;
    document["simulation"] = simulation;
    document["simulationVersion"] = simulationVersion;
    document["engine"] = engineVersion;
    document["configuration"] = configuration;
    document["stepsPerSecond"] = stepsPerSecond;
    document["players"] = start.players;
    document["buttons"] = Strings(buttons);
    document["axes"] = Strings(axes);
    document["seed"] = start.seed;
    document["start"] = start.parameters;
    document["steps"] = steps;
    auto input = JsonIO::Document::MakeArray();
    for (const auto& change : changes) {
        auto item = JsonIO::Document::MakeObject();
        item["at"] = change.at;
        item["player"] = change.player;
        if (change.input.held) item["held"] = Names(change.input.held, buttons);
        if (change.input.pressed) item["pressed"] = Names(change.input.pressed, buttons);
        auto values = JsonIO::Document::MakeObject();
        for (std::size_t i = 0; i < axes.size(); ++i)
            if (change.input.axes[i]) values[axes[i]] = change.input.axes[i];
        if (!values.Members().empty()) item["axes"] = std::move(values);
        input.Append(std::move(item));
    }
    document["input"] = std::move(input);
    auto timed = JsonIO::Document::MakeArray();
    for (const auto& command : commands) {
        auto item = JsonIO::Document::MakeObject();
        item["at"] = command.at;
        item["command"] = command.command.id;
        if (command.command.value) item["value"] = command.command.value;
        timed.Append(std::move(item));
    }
    document["commands"] = std::move(timed);
    document["hashEvery"] = hashEvery;
    document["hashes"] = EncodeHashes(hashes);
    if (!tuning.empty()) {
        auto files = JsonIO::Document::MakeObject();
        for (const auto& digest : tuning)
            files[digest.source] = FormatStateHash(digest.hash);
        document["tuning"] = std::move(files);
    }
    return document;
}
InputTape InputTape::FromDocument(const JsonIO::Document& document) {
    if (document.At("format").AsInt64() != TapeFormat) throw std::runtime_error("Unsupported tape format");
    InputTape tape;
    tape.simulation = document.At("simulation").AsString();
    tape.simulationVersion = IntOf(document.At("simulationVersion"), "simulationVersion");
    tape.engineVersion = document.At("engine").AsString();
    tape.configuration = document.At("configuration").AsString();
    tape.stepsPerSecond = IntOf(document.At("stepsPerSecond"), "stepsPerSecond");
    tape.start.players = IntOf(document.At("players"), "players");
    tape.buttons = ReadStrings(document.At("buttons"));
    tape.axes = ReadStrings(document.At("axes"));
    if (tape.buttons.size() > MaxPlayerButtons || tape.axes.size() > MaxPlayerAxes)
        throw std::runtime_error("Tape declares too many buttons or axes");
    tape.start.seed = document.At("seed").AsUInt64();
    tape.start.parameters = document.At("start");
    tape.steps = document.At("steps").AsUInt64();
    std::uint64_t previous = 0;
    for (const auto& item : document.At("input").Elements()) {
        Change change;
        change.at = item.At("at").AsUInt64();
        change.player = IntOf(item.At("player"), "player");
        if (change.at < previous || change.at >= tape.steps || change.player < 0 || change.player >= tape.start.players)
            throw std::runtime_error("Tape input is out of order or out of range");
        previous = change.at;
        if (const auto* held = Find(item, "held")) change.input.held = ReadBits(*held, tape.buttons);
        if (const auto* pressed = Find(item, "pressed")) change.input.pressed = ReadBits(*pressed, tape.buttons);
        if (const auto* values = Find(item, "axes"))
            for (const auto& [name, value] : values->Members())
                change.input.axes[IndexOf(tape.axes, name)] = IntOf(value, "axes");
        tape.changes.push_back(change);
    }
    previous = 0;
    for (const auto& item : document.At("commands").Elements()) {
        TimedCommand command;
        command.at = item.At("at").AsUInt64();
        if (command.at < previous || command.at >= tape.steps)
            throw std::runtime_error("Tape commands are out of order or out of range");
        previous = command.at;
        command.command.id = item.At("command").AsString();
        if (const auto* value = Find(item, "value")) command.command.value = value->AsInt64();
        tape.commands.push_back(std::move(command));
    }
    tape.hashEvery = IntOf(document.At("hashEvery"), "hashEvery");
    if (tape.hashEvery < 1) throw std::runtime_error("Tape hashEvery must be at least 1");
    tape.hashes = DecodeHashes(document.At("hashes").AsString());
    if (const auto* files = Find(document, "tuning"))
        for (const auto& [source, hash] : files->Members())
            tape.tuning.push_back({source, TuningHash(hash.AsString())});
    return tape;
}
void InputTape::Save(const std::filesystem::path& file) const {
    JsonIO::Write(file, ToDocument(), JsonIO::WriteOptions{.limits = TapeLimits});
}
InputTape InputTape::Load(const std::filesystem::path& file) {
    return FromDocument(JsonIO::Read(file, JsonIO::ReadOptions{.limits = TapeLimits}));
}
