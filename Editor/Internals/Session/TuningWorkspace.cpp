#include "Editor/Internals/Session/TuningWorkspace.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cwctype>
#include "Platform/Internals/FileStorage.h"

namespace {
constexpr auto PollInterval = std::chrono::milliseconds(250);
constexpr std::size_t MaxFileBytes = 4 * 1024 * 1024;

std::optional<std::filesystem::file_time_type> WriteTime(const std::filesystem::path& file) {
    std::error_code error;
    const auto time = std::filesystem::last_write_time(file, error);
    return error ? std::nullopt : std::optional(time);
}

// Worker ile CMake aynı dosyayı farklı yazabilir (ayraç, harf büyüklüğü); karşılaştırma bu anahtarla.
std::wstring Key(const std::filesystem::path& file) {
    std::wstring key = std::filesystem::absolute(file).lexically_normal().generic_wstring();
    std::transform(key.begin(), key.end(), key.begin(), [](wchar_t c) { return std::towlower(c); });
    return key;
}

std::string Utf8(const std::filesystem::path& file) {
    const auto text = file.generic_u8string();
    return {text.begin(), text.end()};
}

void Read(TuningFileState& file) {
    file.written = WriteTime(file.source);
    try {
        file.fields = ParseTuningFile(ReadBinaryFile(file.source, MaxFileBytes));
        file.problem.reset();
    } catch (const TuningFormatError& error) {
        file.problem = TuningFileProblem{error.Line(), error.what()};
    } catch (const std::exception& error) {
        file.problem = TuningFileProblem{0, error.what()};
    }
}

void Label(std::vector<TuningFileState>& files) {
    if (files.empty()) return;
    std::filesystem::path common = files.front().source.parent_path();
    for (const auto& file : files)
        while (!common.empty() && Key(file.source).rfind(Key(common) + L"/", 0) != 0) {
            const auto parent = common.parent_path();
            if (parent == common) break;
            common = parent;
        }
    for (auto& file : files)
        file.label = Utf8(file.source.lexically_relative(common));
}
} // namespace

void TuningWorkspace::Open(const std::filesystem::path& buildDirectory) {
    _list = buildDirectory / "Tuning" / "TuningFiles.json";
    _listWritten.reset();
    _files.clear();
    _polledAt = {};
    Poll();
}

void TuningWorkspace::Poll() {
    if (_list.empty()) return;
    const auto now = std::chrono::steady_clock::now();
    if (now - _polledAt < PollInterval) return;
    _polledAt = now;
    if (WriteTime(_list) != _listWritten) ReadList();
    for (auto& file : _files)
        if (WriteTime(file.source) != file.written) Read(file);
}

void TuningWorkspace::ReadList() {
    _listWritten = WriteTime(_list);
    std::vector<TuningFileState> files;
    try {
        const auto json = nlohmann::json::parse(ReadBinaryFile(_list, MaxFileBytes));
        for (const auto& entry : json.at("files")) {
            TuningFileState file;
            file.source = std::filesystem::path(std::u8string_view(
                reinterpret_cast<const char8_t*>(entry.at("source").get_ref<const std::string&>().data()),
                entry.at("source").get_ref<const std::string&>().size()));
            file.target = entry.at("target").get<std::string>();
            const auto duplicate = std::ranges::find_if(
                files, [&](const TuningFileState& other) { return Key(other.source) == Key(file.source); });
            if (duplicate == files.end()) files.push_back(std::move(file));
        }
    } catch (const std::exception&) {
        files.clear(); // Liste yok ya da bozuk: proje henüz tuning kullanmıyor ya da configure edilmedi
    }
    for (auto& file : files) {
        const auto previous = std::ranges::find_if(
            _files, [&](const TuningFileState& old) { return Key(old.source) == Key(file.source); });
        if (previous != _files.end()) file.worker = previous->worker;
        Read(file);
    }
    std::ranges::sort(files, {}, &TuningFileState::source);
    Label(files);
    _files = std::move(files);
}

std::string TuningWorkspace::Set(std::size_t index, const TuningLocation& at, const TuningValue& value) {
    if (index >= _files.size()) return "Unknown tuning file";
    auto& file = _files[index];
    std::string error;
    try {
        const std::string text = ReadBinaryFile(file.source, MaxFileBytes);
        WriteBinaryFileAtomic(file.source, PatchTuningValue(text, at, value));
    } catch (const TuningFormatError& failure) {
        error = file.label + ":" + std::to_string(failure.Line()) + ": " + failure.what();
    } catch (const std::exception& failure) {
        error = file.label + ": " + failure.what();
    }
    Read(file);
    return error;
}

std::string TuningWorkspace::Report(const PreviewTuningStatus& status) {
    const auto key = Key(std::filesystem::path(
        std::u8string_view(reinterpret_cast<const char8_t*>(status.source.data()), status.source.size())));
    for (auto& file : _files)
        if (Key(file.source) == key) {
            file.worker = status;
            return file.label;
        }
    return {};
}

void TuningWorkspace::ClearWorkerStatus() {
    for (auto& file : _files)
        file.worker.reset();
}
