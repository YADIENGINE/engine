# Önizleme geliştirmek

## İçerik türünü seçmek

Basit çizim için `EditorPreview::Draw() const` yeterlidir. Animasyon/durum için
`Update(dt)`, kullanıcı etkileşimi için `HandleInput`, trigger için `ExecuteAction` ekleyin.
Her katalog girişi bir `PreviewDefinition` ve fabrika ile açıkça kaydedilir.
Genel engine otomatik dosya tarayarak preview kaydetmez.
Projeler kendi kısa kayıt/galeri yardımcılarını (hızlı çizim, galeri sahnesi gibi) yazabilir;
EngineEditor API'si kullanmak için bunlar gerekmez.

Önizleme üretim model/View'ını çağırır. Aynı görünüşün ikinci bir editör sürümünü yazmayın.
Liste/galeri düzeni ve proje içi kısa kayıt yardımcıları tüketicinin geliştirme katmanına aittir.

## Kaynak ve lifecycle

1. Registry yalnız metadata ve create callback'i içerir; global kayıtta GL/font/ses oluşturulmaz.
2. Fabrika servisler hazırken engine thread'inde tam parametre haritasıyla örneği kurar.
3. ObjectList Start/Update/Draw/Stop'u yönetir; içerik bu hook'ları elle dispatch etmez.
4. Reset kabul edilen tarifle yeni örnek oluşturur; eski kaynaklar context yaşarken temizlenir.

`PreviewServices` metin/ses referansları örnekten uzun yaşar. View'lar texture sahipliğini
açıkça tutar; dependent efektler texture'dan önce temizlenir.
Stop/destructor exception atmaz. Fabrika/hook istisnası worker'ı kapatabilir.

## Kontrol tasarımı

Parametre türü, default, sınır, grup ve açıklamayı tek definition'da author edin.
Kimlik sabittir; favori/geçmiş ve kabul edilen override'lar ona bağlıdır.
Float değeri `double`, integer `int64_t`, Choice seçenek metnidir.
`Immediate` görünüşü değiştirir; `NextAction` sonraki tetiklemeye hazırlar;
`OnReset` yeni örnekte kullanılır. Constructor başlangıç değerlerini kendisi okur;
ilk kurulumda setter çağrıldığını varsaymayın.

Rastgele davranış için açık seed parametresi sağlayın. Randomize/ürün girdisi bir
authored kontrolü değiştiriyorsa CaptureParameters ile geri bildirin.
Canlı sayaç, transient animasyon ve hata açıklaması Inspect içindir; recipe'ye yazılmaz.
Ret açıklaması döndüren setter mevcut içeriği değiştirmemelidir.

## Saat, input ve gerçek oyun akışı

Dekoratif zaman `DrawTime()`, durum ilerletme zamanı Update dt'dir.
Sahne saatini View'lara elle bağlamayın; Pause, Step ve hız host tarafından uygulanır.
Rastgele zamana scrub desteklenmez; stateful preview için Reset + Step kullanın.
View alanına odaklanıldığında ürün input'u gelir; Inspector odağını veya OS input'unu yeniden okumayın.

Gerçek oyunu çalıştıran önizleme için RunPreviewHost'a özel Game fabrikası verilebilir.
`PreviewSession` kontrol, lifecycle ve kare aktarımını taşır; oyun sahneleri aynı
Game::Run döngüsünde kalır. Transient profil, kayıt izolasyonu ve test akışı tüketicinin sorumluluğudur.
`persistentOptions=false` kendi başına oyun ilerlemesi/telemetry yazımını engellemez.
Üretim Game'e Editor include veya editöre özel koşullu kod eklemek yerine genel,
paylaşılabilir sahne/View girişleri tasarlayın.

Tam değer ve IPC sözleşmesi: [API](Api.md). Doğrulama: [Testing](Testing.md).
