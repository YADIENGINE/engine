#pragma once
#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>
#include <vector>
#include "Utilities/Color4.h"
#include "Utilities/Float2.h"
#include "Utilities/FloatRange.h"

// Tuning dosyası biçimi (Docs/Tuning.md): okuma, doğrulama ve tek değer yamama. Canlı çalışma zamanı ve editör
// kullanır. Derleme sırasındaki üretici aynı kuralları Python'da uygular (Tools/tuning/document.py); iki tarafı
// Tools/Tests/TuningCases'teki ortak örnekler denetler. Kurallardan birini değiştiren ikisini birlikte değiştirir.

// Tek değerli tipler; liste ve tablo sütunu bunlardan kurulur. Sıra TuningValue'nun alternatif sırasıdır.
enum class TuningType { Float, Int, Bool, Color, Vec2, Range };
using TuningValue = std::variant<float, int, bool, Color4, Float2, FloatRange>;

[[nodiscard]] std::string_view TuningTypeName(TuningType type);

// min / max geçerlilik sınırıdır; listede her elemana, vec2 / range'de her sayıya uygulanır.
struct TuningSpec {
    TuningType type = TuningType::Float;
    bool list = false;
    std::optional<double> min;
    std::optional<double> max;
};

struct TuningColumn {
    std::string name;
    TuningSpec spec;
};

// Değer / liste alanı: spec + values (tek değerde bir eleman). Tablo: columns + rows; rows[satır][sütun] o hücrenin
// değerleridir (liste sütununda birden çok).
struct TuningField {
    std::string name;
    int line = 1;
    bool table = false;
    TuningSpec spec;
    std::vector<TuningValue> values;
    std::vector<TuningColumn> columns;
    std::vector<std::vector<std::vector<TuningValue>>> rows;
};

// Bir değerin yeri: alan; tabloda satır + sütun ('column' boş değilse); listede eleman.
struct TuningLocation {
    std::string field;
    std::size_t row = 0;
    std::string column;
    std::size_t element = 0;
};

// Dosyadaki ilk hata; satır 1'den başlar.
class TuningFormatError final : public std::runtime_error {
public:
    TuningFormatError(int line, const std::string& text);
    [[nodiscard]] int Line() const { return _line; }

private:
    int _line;
};

// Doğrulanmış alanlar, dosyadaki sırayla.
[[nodiscard]] std::vector<TuningField> ParseTuningFile(std::string_view text);
// Geçerli 'text'te yalnız o değerin yazımını değiştirir; geri kalan bayt bayt aynı kalır. Dosya ya da sonuç geçersizse
// (tip uyuşmazlığı, sınır dışı) TuningFormatError.
[[nodiscard]] std::string PatchTuningValue(std::string_view text, const TuningLocation& at, const TuningValue& value);
