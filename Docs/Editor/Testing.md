# Editor validation

Komutlar geliştirici içindir; yalnız belge değişikliğinde build/test gerekmez.
Headless altyapı test komutları ve kapsamı [API](Api.md#headless-editör-testleri) içindedir.
Gerçek pencere/input/görsel kontrolleri kullanıcı yapar; yalnız değişen bölüm seçilir.

## Önizleme sözleşmesi

1. Kayıt GL/font/ses oluşturmadan katalog alınabilmeli.
2. Başlangıç recipe'si fabrikaya tam gelmeli; geçersiz değer atomik reddedilmeli.
3. Immediate / NextAction / OnReset, ret, Default ve capture Inspector'a doğru yansımalı.
4. Pause dekoratif saat ve Update'i dondurmalı; Step 1/60 s, Reset saat 0 olmalı.
5. Reset/Stop/yeniden açma kaynakları eski context kapanmadan bırakmalı.
6. Başka projeye ait oyuncu settings/progress/telemetry dosyaları değişmemeli.

## Editör arayüzü

1. Panelleri dock et / sekmede birleştir; kapatıp Windows menüsünden aç. Live preview'u büyüt / geri dön.
2. %75 / %100 / %125 / %150 / %200 yazı boyutu ve monitör DPI değişimi; dar panellerde uzun adlar satıra bölünüyor.
3. Favori yıldızı içerik açmıyor; Recent yalnız başarılı açılışları sıralıyor.
4. Inspector grupları, açıklamalar, pending / ret ve Default; OnReset için Reset.
5. Output filtre / arama / Ctrl seçim / kopyalama / temizleme; yukarı kaydırınca takip duruyor; gizliyken hata rozeti.
6. Yeniden açılışta tercihler dönüyor; büyütülmüş halde kapatmak önceki düzeni koruyor.
7. Önizleme sekmesi gizliyken, başka panel arkasındayken veya menü açıkken ürüne input gitmiyor.
8. Buff Visuals: Gain / refresh tam genişlikte; Replay / End effects ve Special action / Combine buffs çiftleri
   altta. Daraltınca tek sütun; bütün eylemler ve disabled durumları korunuyor.
9. Buff Visuals, Card Inspector, UI Buttons ve Game Flows: iki sütun hizası, uzun etiket / seçenek ipuçları,
   sayı / renk düzenleme, klavye odağı, bölüm açma / kapama ve sağa hizalı canlı değerler. %75..%200 ve dar
   Inspector'da taşma yok; Default ve uygulama zamanı kontrol sütununda okunuyor.

## Altyapı

1. Debug derle; uyarı / hata yok. İkinci görünür worker penceresi / konsolu yok.
2. Görüntü merkez / köşe / kenar boşluğu tıklamaları; dışarı sürükleyip bırakma; panel, pencere ve DPI değişimi.
3. Inspector'a yazarken klavye önizlemeye gitmiyor; alt-tab sonrası basılı girdi temizleniyor.
4. Tek Open / Stop düğmesi önizleme açılınca (Pause dahil) Stop, kapandıktan sonra Open gösteriyor; açılışta Stop kullanılabiliyor. Stop / Open / Reset tekrarlarında editör PID'i sabit; eski görüntü canlı sayılmıyor.
5. Derleme hatası, eksik exe, worker çökmesi, build iptali ve editör kapanışı: orphan süreç kalmıyor.
6. Options'taki FPS ve readback / publish / transfer ölçüleri kaydediliyor.

## Profiler

Headless modül testleri `[profiler]` etiketiyle istatistikler, scope nesting, eksik GPU örneği, sınırlar,
rapor IO ve karşılaştırma koşullarını doğrular. Editor testlerinde warmup/measurement ayrımı, A/B tekrar
sırası, iptal, yürütme ayarı snapshot/restorasyonu, benchmark metadata round-trip ve stale generation/capture
mesajları kontrol edilir. Testler gerçek GL/FPS sonucu sabitlemez.
Bu testleri derleme/çalıştırma geliştiriciye aittir. Runtime ayar restorasyonu ve GPU backend'i için
[Profiler manuel kabul listesi](Profiler.md#manual-acceptance) kullanılır.
