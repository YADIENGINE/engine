#pragma once
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>
#include "Editor/Internals/Protocol/PreviewProtocol.h"
#include "Tuning/TuningFile.h"

struct TuningFileProblem {
    int line = 0;
    std::string text;
};

struct TuningFileState {
    std::filesystem::path source;
    std::string target;
    std::string label; // Bütün tuning dosyalarının ortak klasörüne göre yol
    std::vector<TuningField> fields;
    std::optional<TuningFileProblem> problem;  // Dosya okunamıyor ya da geçersiz; fields son geçerli hali
    std::optional<PreviewTuningStatus> worker; // Önizlemenin son raporu; önizleme dosyayı kullanmıyorsa yok
    std::optional<std::filesystem::file_time_type> written;
};

// Projenin tuning dosyaları editörde (Docs/Tuning.md). Liste build'in Tuning/TuningFiles.json'undan (CMake
// engine_add_tuning), değerler diskten okunur ve dosya dışarıdan değişince tazelenir. Dosya tek yazma yoludur:
// Set değeri o anki diskteki dosyaya yamar ve atomik yazar; çalışan önizleme dosyayı kendisi izler ve sonucunu
// bildirir (Report).
class TuningWorkspace {
public:
    void Open(const std::filesystem::path& buildDirectory);
    // Liste ve dosyalar değiştiyse yeniden okur; en çok 250 ms'de bir diske bakar.
    void Poll();
    [[nodiscard]] const std::vector<TuningFileState>& Files() const { return _files; }
    // Boş metin: yazıldı. Değilse dosyaya dokunulmadı (dosya o an geçersiz, değer sınır dışı ya da tip yanlış).
    [[nodiscard]] std::string Set(std::size_t file, const TuningLocation& at, const TuningValue& value);
    // Önizlemenin raporu; dosyalardan biriyse onun durumu olur. Dosyanın etiketi, bilinmiyorsa boş.
    std::string Report(const PreviewTuningStatus& status);
    void ClearWorkerStatus();

private:
    void ReadList();

    std::filesystem::path _list;
    std::optional<std::filesystem::file_time_type> _listWritten;
    std::vector<TuningFileState> _files;
    std::chrono::steady_clock::time_point _polledAt{};
};
