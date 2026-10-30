#pragma once
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

// Tuning dosyaları için konumlu, katı JSON okuyucu: her değer satırını ve metindeki bayt aralığını taşır (hata
// satırı ve tek değer yamama için). Yorum, sondaki virgül, tekrar eden anahtar hatadır (TuningFormatError).
enum class TuningJsonKind { Object, Array, String, Number, Bool, Null };

struct TuningJsonMember;

struct TuningJsonNode {
    TuningJsonKind kind = TuningJsonKind::Null;
    int line = 1;
    std::size_t begin = 0; // Metindeki yazımı: [begin, end)
    std::size_t end = 0;
    std::string text; // String: çözülmüş metin; Number: yazıldığı gibi
    bool boolean = false;
    std::vector<TuningJsonMember> members; // Object, yazıldığı sırayla
    std::vector<TuningJsonNode> items;     // Array

    [[nodiscard]] const TuningJsonNode* Find(std::string_view key) const;
};

struct TuningJsonMember {
    std::string key;
    int line = 1;
    TuningJsonNode value;
};

[[nodiscard]] TuningJsonNode ParseTuningJson(std::string_view text);
