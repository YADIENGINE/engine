#pragma once
#include <filesystem>
#include <span>

// Uygulamanın kendi dosyaları: paketli asset'leri doğrular; kayıt yolunu applicationId altında
// platformun kullanıcı verisi dizininde çözer. Runtime açılışta kullanır, oyun Game::SavePath üzerinden ulaşır.
[[nodiscard]] std::filesystem::path PrepareAssets(std::span<const std::filesystem::path> files);
[[nodiscard]] std::filesystem::path SaveFilePath(const std::filesystem::path& applicationId,
                                                 const std::filesystem::path& filename);
