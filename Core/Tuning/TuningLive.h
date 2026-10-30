#pragma once
// Tuning değerlerinin canlı tarafı (Docs/Tuning.md): yalnız geliştirme derlemelerinde (ENGINE_TUNING_LIVE) vardır.
// Üretilen struct'ın alanları çalışma zamanına tanıtılır; dosya değişince Game her karenin başında PollTuning ile
// okur ve ya hep ya hiç uygular. Release'de bu header boştur, okuma / izleme kodu derlenmez.
#if ENGINE_TUNING_LIVE
#include <array>
#include <cstddef>
#include <filesystem>
#include <functional>
#include <span>
#include <string>
#include <string_view>
#include <vector>
#include "Tuning/TuningFile.h"

template <class T> struct TuningTypeOf;
template <> struct TuningTypeOf<float> {
    static constexpr TuningType Value = TuningType::Float;
};
template <> struct TuningTypeOf<int> {
    static constexpr TuningType Value = TuningType::Int;
};
template <> struct TuningTypeOf<bool> {
    static constexpr TuningType Value = TuningType::Bool;
};
template <> struct TuningTypeOf<Color4> {
    static constexpr TuningType Value = TuningType::Color;
};
template <> struct TuningTypeOf<Float2> {
    static constexpr TuningType Value = TuningType::Vec2;
};
template <> struct TuningTypeOf<FloatRange> {
    static constexpr TuningType Value = TuningType::Range;
};

// Derlenmiş struct'taki bir alan. Değer / liste: 'data' 'length' ardışık elemanı gösterir. Tablo: satır başına
// sütunların kendi referansları.
struct TuningFieldRef {
    std::string_view name;
    TuningType type = TuningType::Float;
    bool list = false;
    std::size_t length = 1;
    void* data = nullptr;
    bool table = false;
    std::vector<std::vector<TuningFieldRef>> rows;
};

// Üretilen VisitTuning'in ziyaretçisi: alanları TuningFieldRef listesine çevirir.
class TuningCollector {
public:
    explicit TuningCollector(std::vector<TuningFieldRef>& fields)
        : _fields(fields) {}

    template <class T> void Field(std::string_view name, T& value) {
        _fields.push_back({name, TuningTypeOf<T>::Value, false, 1, &value});
    }
    template <class T, std::size_t N> void Field(std::string_view name, std::array<T, N>& values) {
        _fields.push_back({name, TuningTypeOf<T>::Value, true, N, values.data()});
    }
    template <class Row, std::size_t N> void Table(std::string_view name, std::array<Row, N>& rows) {
        TuningFieldRef field{name};
        field.table = true;
        for (auto& row : rows) {
            TuningCollector columns(field.rows.emplace_back());
            row.VisitTuning(columns);
        }
        _fields.push_back(std::move(field));
    }

private:
    std::vector<TuningFieldRef>& _fields;
};

// Applied: değerler yazıldı. Invalid: dosya geçersiz, son geçerli değerler sürer. NeedsBuild: alan eklendi / silindi /
// tipi ya da uzunluğu değişti, derleme gerekir. Unreadable: dosya okunamadı.
enum class TuningOutcome { Applied, Invalid, NeedsBuild, Unreadable };

struct TuningReport {
    std::string source; // Dosya yolu, UTF-8
    TuningOutcome outcome = TuningOutcome::Applied;
    int line = 0; // Invalid'de hatanın satırı
    std::string text;
    bool reload = false; // false: açılıştaki ilk okuma
};

// Metni derlenmiş alanlara karşı doğrular; her şey geçerli ve yapı aynıysa bütün değerleri yazar, yoksa hiçbirine
// dokunmaz. Dönen raporun source / reload alanları boştur.
[[nodiscard]] TuningReport ApplyTuningText(std::string_view text, std::span<const TuningFieldRef> fields);
// "dosya:satır: ..." biçiminde tek satır.
[[nodiscard]] std::string DescribeTuningReport(const TuningReport& report);

// Kayıtlı bir tuning struct'ı; Tuning<T>() TuningSlot<T> olarak kurar.
class TuningBinding {
public:
    virtual ~TuningBinding() = default;
    [[nodiscard]] virtual std::filesystem::path Source() const = 0;
    [[nodiscard]] virtual TuningReport Apply(std::string_view text) = 0;

protected:
    TuningBinding() = default;

public:
    TuningBinding(const TuningBinding&) = delete;
    TuningBinding& operator=(const TuningBinding&) = delete;
};

// Kaydı ekler ve dosyayı hemen bir kez okur (açılış değeri derlendiği andaki değerdir; dosya sonra değiştiyse o).
void RegisterTuning(TuningBinding& binding);
void UnregisterTuning(TuningBinding& binding);
// Kayıtlı dosyalardan değişenleri yeniden uygular; Game her karenin başında çağırır (ana iş parçacığı). Dosya
// durumuna en çok 200 ms'de bir bakar.
void PollTuning();
// Raporların gideceği yer; boş fonksiyon varsayılanı (stderr) geri yükler. Varsayılan, açılıştaki başarılı okumayı
// yazmaz.
void SetTuningReporter(std::function<void(const TuningReport&)> reporter);

template <class T> class TuningSlot final : public TuningBinding {
public:
    TuningSlot() { RegisterTuning(*this); }
    ~TuningSlot() override { UnregisterTuning(*this); }

    [[nodiscard]] const T& Value() const { return _value; }
    [[nodiscard]] std::filesystem::path Source() const override { return std::filesystem::path(T::TuningSource); }
    [[nodiscard]] TuningReport Apply(std::string_view text) override {
        std::vector<TuningFieldRef> fields;
        TuningCollector collector(fields);
        _value.VisitTuning(collector);
        return ApplyTuningText(text, fields);
    }

private:
    T _value{};
};
#endif
