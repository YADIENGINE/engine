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

1. İlk tercihlerde Develop: Project / View / Inspector açık, Output / Profiler kapalı. Panelleri dock et / sekmede
   birleştir; kapatıp View menüsünden aç. View menüsüyle Fit / Focus / 100%, zoom ve geçici büyüt / geri dön çalışıyor.
2. %75 / %100 / %125 / %150 arayüz ölçeği ve monitör DPI değişimi; dar panellerde uzun adlar sarılıyor veya
   tooltip ile kısalıyor. Toolbar'ın ikincil eylemleri taşma menüsünde; yatay toolbar kaydırması yok.
3. Ctrl+P Project aramasını, gizliyken de, açıp odaklıyor. Favori yıldızı içerik açmıyor; Recent yalnız başarılı
   açılışları sıralıyor. Boş arama / favori / geçmiş durumları anlamlı açıklama gösteriyor.
4. Inspector Search / Modified sayacı, `?` açıklaması, değişmiş değer işareti, Pending / ret ve tek alan sıfırlama;
   seçenek grupları tam genişliği kullanır. Paneli daraltınca Phase gibi üçlü seçenekler daha az sütuna iner;
   normal kontroller etiketin altına geçer, checkbox sığdığında aynı satırda kalır. Düğmelerin metni dikey harf sütunlarına dönüşmez.
   OnReset için Reset. Uygulama zamanı grup başına tek açıklama veya karışık grupta `*` tooltip'iyle gösterilir.
   Değişmemiş alanın reset'i hover / klavye odağında görünür; yerleşim oynamaz. İleri gruplar önce kapalıdır,
   arama / Modified eşleşmeleri açar. İçerik tanımındaki grup, primary ve disabled metadata'sı korunuyor.
5. Output Last Build Try / Full ayrımı, kapsama göre sayaç / arama / Copy Visible; tıklama / Ctrl seçim /
   Shift aralığı / Ctrl+A / Ctrl+C / sağ tık kopyalama / çift tıklama metin ayrıntısı. Arama ve ayrıntıda
   metin kopyalama korunuyor; yukarı kaydırınca veya seçim yapınca Follow duruyor. Clear yalnız Full
   tamponunu siliyor; Open Log bütün oturum veya deneme çıktısını açıyor. Uzun çıktıda ilk hata korunuyor.
   Başarısız build'de popup bir kez geliyor; Show Output kapalı veya büyütülmüş düzenden Output'a
   dönüp ilk hatayı gösteriyor, eski arama ve kapalı Error filtresi hatayı gizlemiyor. İptalde popup yok;
   başarılı derleme sonrası worker başlatma hatası ayrı anlatılıyor. Yeni deneme önceki denemeye karışmıyor.
6. Develop / Inspect / Profile arasında geçiş son özel düzeni silmiyor; Last custom layout geri getiriyor.
   Ctrl+J ve alt çubuk son aracı açıp kapatıyor. Yeniden açılışta tercihler dönüyor; büyütülmüş halde kapatmak
   önceki düzeni koruyor.
7. Önizleme sekmesi gizliyken, başka panel arkasındayken veya menü açıkken ürüne input gitmiyor.
8. Primary eylem tam genişlikte ve kaydırırken panelin altında sabit; ikincil eylemler yeterli genişlikte iki,
   dar panelde tek sütun. Uzun etiket / seçenek ipuçları, sayı / renk düzenleme ve bölüm açma / kapama taşmıyor.
   Canlı sayılar sağa hizalı, kısa durumlar rozetli, uzun açıklamalar satır sarıyor; child alanda yatay taşma yok.
9. Ctrl+Shift+P ile paleti aç: arama, yukarı / aşağı, Enter, Escape, kapalı komut nedeni, panel / layout seçimi.
   F5 / F6 / F7 / Ctrl+R / Ctrl+Shift+B toolbar ve menüyle aynı kullanılabilirliği izliyor; metin / popup
   sırasında çalışmıyor ve ürüne sızmıyor.

## Altyapı

1. Debug derle; uyarı / hata yok. İkinci görünür worker penceresi / konsolu yok.
2. Görüntü merkez / köşe / kenar boşluğu tıklamaları; dışarı sürükleyip bırakma; panel, pencere ve DPI değişimi.
3. Inspector'a yazarken klavye önizlemeye gitmiyor; alt-tab sonrası basılı girdi temizleniyor.
4. Ana düğme kapalıyken Open, hazır / pause'dayken Stop, açılış / katalog / derleme / benchmark sırasında Cancel,
   kapanırken devre dışı Stop gösteriyor. Stop / Open / Reset tekrarlarında editör PID'i sabit; eski görüntü canlı sayılmıyor.
5. Derleme hatası, eksik exe, worker çökmesi, build iptali ve editör kapanışı: orphan süreç kalmıyor.
6. View içindeki performans ölçüsü tercihi kaydediliyor; FPS ve readback / publish / transfer değerleri görünüyor.

## Profiler

Headless modül testleri `[profiler]` etiketiyle istatistikler, scope nesting, eksik GPU örneği, sınırlar,
rapor IO ve karşılaştırma koşullarını doğrular. Editor testlerinde warmup/measurement ayrımı, A/B tekrar
sırası, iptal, yürütme ayarı snapshot/restorasyonu, benchmark metadata round-trip ve stale generation/capture
mesajları kontrol edilir. Testler gerçek GL/FPS sonucu sabitlemez.
Bu testleri derleme/çalıştırma geliştiriciye aittir. Runtime ayar restorasyonu ve GPU backend'i için
[Profiler manuel kabul listesi](Profiler.md#manual-acceptance) kullanılır.
UI kontrolünde Advanced benchmark settings, Glow override, tek Export menüsü ve benchmark iptali de seçilir.
