# Run statistics storage

Engine run kurallarını veya kayıt şemasını tanımlamaz. `Tracking/RunStatisticsSys`
oyunun hazırladığı JsonIO::Document'ı durable gzip JSON dosyası olarak yazar.
Kaynak: [RunStatisticsSys.h](../Tracking/RunStatisticsSys.h).

```cpp
#include "Tracking/RunStatisticsSys.h"
#include "Modules/JsonIO/JsonIO.hpp"

// Game türevinin bir kayıt metodunda:
RunStatisticsSys archive(SavePath("runs"));
const auto runId = RunStatisticsSys::NewRunId();
auto record = JsonIO::Document::MakeObject();
record["run_id"] = runId;
record["ended_at_utc"] = RunStatisticsSys::UtcNow();
record["score"] = 120; // örnek oyunun kendi alanı
const auto file = archive.Write(runId, record);
```

`NewRunId()` run başında bir kez alınır ve retry için aynı ID saklanır.
`Write` dosya adını `<run_id>.json.gz` yapar; aynı ID hedefi atomik değiştirir,
ikinci bir kayıt oluşturmaz. Geçerli bir dosya adı olarak NewRunId çıktısını kullanın.
`Directory()` arşiv kökünü verir. Yazma JsonIO üzerinden compact JSON + gzip üretir;
Persistence'ın backup/kurtarma politikası uygulanmaz.

Yazma senkrondur ve hata fırlatabilir; oyunun hata sınırında yakalayın.
Retry kuyruğu, run bitiş tespiti, alanlar, şema sürümü, filtre, retention ve upload oyun politikasıdır.
Run boyunca otomatik snapshot veya crash kurtarma yoktur.
Bu helper JsonIO maxBytes sınırını size_t üst sınırına çıkarır; varsayılan 1 MiB sınırı uygulanmaz.
Varsayılan nesting limiti ve gzip compressor boyut sınırları devam eder.
Run ID 1..120 ASCII harf/rakam/tire/alt çizgi olmalıdır; diğer değerler invalid_argument üretir.
Gzip standart araçlarla açılır; JsonIO::Read gzip açmaz.

`EngineGodAccessor::RunStatistics::Context(game)` ve `Directory(game)` opsiyonel
salt okunur application bağlamı sağlar. Bunlar oyunun skor/oyuncu alanlarını doldurmaz.
Kayıt bütünlüğü ve sıkıştırma sınırları: [JsonIO](Modules/JsonIO/README.md);
bağımlılık lisansları: [ThirdParty](Modules/JsonIO/ThirdParty.md).
