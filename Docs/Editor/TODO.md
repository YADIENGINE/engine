# Editör UI geliştirme listesi

Bu liste mevcut özellikleri değil, sonraki yetenekleri ve gerekli altyapı sırasını tutar.
Uygulanan arayüz için [Usage](Usage.md), sınırlar için [geliştirme notları](Development/Roadmap.md) kullanılır.
Bir yetenek uygulanmadan boş düğme, servis veya kullanılmayan public API eklenmez.

| Sonraki yetenek | Önce gereken sözleşme / altyapı |
| --- | --- |
| Kaydedilebilir parametre recipe preset'leri | Önizleme kimliği ve doğrulanmış parametrelerden oluşan kalıcı recipe biçimi; uyumluluk ve yükleme / kaydetme davranışı. Sonra Project / Inspector üzerinden preset seçimi |
| Birden çok önizleme sekmesi / bağlamı | Her bağlamın kendi seçimi, parametreleri, inspection'ı, input odağı ve kaynak sahipliği; ardından eşzamanlı worker ihtiyacı değerlendirilir, gerekiyorsa bağımsız worker lifecycle ve komut hedefleme eklenir |
| Görsel A/B capture ve karşılaştırma | Kaynağı ve önizleme bağlamı belli, değişmeden tutulan capture'lar; ardından yan yana görüntü / karşılaştırma sunumu. Mevcut Profiler A/B benchmark'ı ayrı bir özelliktir |
| Sahne / asset tarayıcısı ve gizmo'lar | Projenin sunduğu kararlı nesne / asset kimliği, seçim ve düzenlenebilir özellik / transform sözleşmesi; ardından ortak seçim modeli, paneller ve viewport araçları |
| Save, dirty durumu, Undo / Redo | Kaynak belge ve kaydetme sahipliği, düzenleme işlemlerinin sınırı ve geri alınabilir komutlar; ardından değiştirilmiş belge durumu, kayıt ve geçmiş arayüzü |
| Zaman çizgisinde scrub | Yeniden kurulabilir deterministik durum veya zaman damgalı snapshot / yeniden simülasyon sözleşmesi; ardından seek ve timeline. Mevcut Pause / Reset / Step rastgele zamana atlama sağlamaz |
| Birden çok işletim sistemi penceresi | Platform / render backend'inde ayrı pencereler, GL bağlamı, monitör DPI'ı ve input odağı yaşam döngüsü; ardından ana pencere dışına taşınabilen dock'lar |

Yeni yetenek mevcut komut ve panel kayıtlarına bağlanır. Engine genel sözleşmeleri sahiplenir; proje içeriği ve
kaynak düzenleme kuralları tüketici adaptöründe kalır. Kullanım / mimari belgesi ve değişen davranışın en küçük
doğrulama senaryosu yetenekle birlikte güncellenir.
