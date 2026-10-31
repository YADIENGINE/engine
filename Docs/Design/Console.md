# Console paneli (tasarım)

Durum: aşama 1 uygulandı (2026-10-31), çalışma zamanı doğrulaması bekliyor; aşama 2 sırada. Karar kaydı: [README → D18](README.md#d18-console-paneli-editörde-gerçek-terminal).

## Amaç

Editörün altında VS Code'un entegre terminali gibi gerçek bir terminal. İçinde kabuk çalışır; kullanıcı AI CLI'ını
(ör. Claude Code), `git`'i ya da `<engine> build`'i oradan çalıştırır. Editör bir AI'a özel arayüz yazmaz: çalışan
program kendi TUI'siyle olduğu gibi görünür, bütün özellikleri (komutlar, izin soruları, oturum) eksiksiz çalışır ve
güncellemeleri kendiliğinden gelir. D6 sayesinde konsoldan verilen `build` editörün derlemesine gider.

## Kapsam

| Aşama | İçerik |
| --- | --- |
| 1. MVP | Tek oturum; ConPTY + libvterm; renkli çizim, imleç, geniş karakter; klavye, Türkçe karakter, yapıştırma; yeniden boyutlama; çıkınca yeniden başlatma |
| 2. Konfor | Kaydırma geçmişi (tekerlek, kaydırma çubuğu, alta yapışma); fareyle seçim, çift tıkla kelime, kopyalama; sağ tık menüsü |

Kapsam dışı (şimdilik): birden fazla sekme / oturum, dosya yolu ve URL algılama, programa fare iletimi (mouse
reporting), metin arama, hazır komut düğmeleri, MCP köprüsü ([Agent API](AgentApi.md) ayrı iş), Windows dışı platform.

## Kararlar

- **Ayrı panel:** `EditorPanel::Console`. Alt dock'ta `Output` ile aynı sekme grubunda; `Ctrl+J` alt alanı açıp
  kapatmaya devam eder. Hazır layout'lara (`Develop`, `Inspect`, `Profile`) eklenir, panel menüsü ve komut
  paletinden açılır.
- **Sadece kabuk:** Panel düz bir kabuk açar, kullanıcı ne çalıştıracağını kendisi yazar. Engine'e AI adı ya da AI
  komutu gömülmez (D14: herkes kendi AI'ından sorumlu).
- **Ortak katman:** Terminal çekirdeği editöre değil, engine kökünde ayrı bir hedefe (`Terminal/`, `EngineTerminal`)
  girer; launcher ileride aynı paneli alabilir. Oyuna girmez.
- **ConPTY + libvterm:** Windows'un sahte konsolu (`CreatePseudoConsole`, VS Code'un node-pty'sinin altındaki API)
  ve libvterm (MIT, C99; Neovim'in gömülü terminalinin kökeni). Kendi VT ayrıştırıcımızı yazmayız.

## Yapı

```
Terminal/                    EngineTerminal (EngineToolUi + libvterm'e link'lenir)
  TerminalSession.h/.cpp     Public: Start(options), Pump(), Write, Resize, Restart, durum sorguları (çalışıyor mu,
                             çıkış kodu, başlık). Ekranı ve geçmişi sahiplenir.
  TerminalView.h/.cpp        ImGui çizimi ve input (hücre ızgarası, imleç, seçim, kaydırma, klavye, yapıştırma)
  Internals/
    PseudoConsole.h/.cpp     ConPTY + pipe'lar + süreç + job object + okuyucu thread
    VtScreen.h/.cpp          libvterm sarmalayıcısı: hücreler, imleç, başlık, alt ekran, tuş kodlama
    Scrollback.h/.cpp        Sınırlı satır halkası (aşama 2)
  Tests/                     EngineTerminalTests (etiket `terminal`)
ThirdParty/libvterm/         Değiştirilmemiş kaynak; ayrı STATIC hedef, uyarılar kapalı (EngineToolImGui gibi)
Editor/Ui/Panels/Console/    ConsolePanel: TerminalSession + TerminalView değer üyeleri, araç çubuğu
```

`TerminalSession` ImGui bilmez (ekran ve süreç mantığı headless test edilir); `TerminalView` onu okur ve input'u
ona yazar. `KeyEncoding` ayrı dosya olmadı: kodlama libvterm'in işi, ConPTY'ye özel istisnalar `VtScreen`'de.
Handle'lar `PseudoConsole`'da `unique_ptr` + kapatıcıyla tutulur; editörün `NativeHandle`'ı `Editor/Internals`'ta
olduğu için kullanılmadı (ortak bir Windows yardımcı katmanı çıkarsa ikisi birleşir).

Kabuğun çalışma klasörü editör tarifinden gelir: `EngineEditor.project.json`'a `sourceDirectory` (CMake'in
`CMAKE_SOURCE_DIR`'ı) eklendi; eski tarifte alan yoksa editörün çalışma klasörü kullanılır.

## Süreç ve thread'ler

- **Kabuk:** `pwsh.exe` PATH'te varsa o, yoksa Windows PowerShell. Editör ayarlarında `console.shell` ile komut
  satırı değiştirilebilir. Çalışma klasörü proje kökü; ortam editörünkü + `COLORTERM=truecolor`.
- **Yaşam döngüsü:** Panel ilk kez görünür olunca başlar. Süreç çıkınca panel çıkış kodunu yazar; Enter ya da
  araç çubuğundaki "Restart" yeniden başlatır. Editör kapanınca oturum kapanır.
- **Job object** (`KILL_ON_JOB_CLOSE`): kabuğun başlattığı her şey (AI CLI'ı, node, derleyici) editörle birlikte ölür,
  arkada yetim süreç kalmaz.
- **Okuyucu thread:** Çıktı pipe'ını okur, kilitli bir kuyruğa yazar. UI thread her karede `Pump()` ile kuyruğu
  libvterm'e verir (libvterm thread güvenli değil, sadece UI thread'i dokunur). Editör sabit 60 FPS çizdiği için
  ayrıca uyandırma gerekmez.
- **Yazma:** Tuşlar ve libvterm'in cevapları (imleç konumu sorgusu vb.) input pipe'ına UI thread'inden yazılır. Büyük
  yapıştırma parçalara bölünür; pipe tıkanırsa bir yazıcı thread'e geçilir.
- **Kapanış sırası:** `ClosePseudoConsole` çıktı pipe'ı boşaltılmazsa kilitlenebilir. Okuyucu thread kapanış bitene
  kadar okumaya devam eder, sonra join edilir; sıra RAII üye sırasıyla sabitlenir.

## Çizim

- **Font:** ToolUi'nin teknik fontu (Consolas, yoksa ImGui varsayılanı); AI TUI'lerinin sembolleri (`⏺ ⎿ ✻`, oklar,
  braille) için teknik fonta Segoe UI Symbol birleştirildi (Output da yararlanır). Kalın kesim yok: kalın metin
  1 px kaydırılıp iki kez çizilir. Düz ASCII koşu olarak, diğer karakterler kendi hücresine hizalı tek tek çizilir.
- **Hücre boyutu** fontun `M` genişliği ve satır yüksekliğinden; sütun / satır panel içeriğinden hesaplanır, değişince
  `Resize` (karede en çok bir kez).
- **Satır başına birleştirme:** Aynı özellikli komşu hücreler tek arka plan dikdörtgeni + tek metin çağrısı.
- **Renkler:** İlk 16 renk ToolTheme'den (koyu temaya uyumlu ANSI paleti), 256 renk ve RGB libvterm'den. Kalın,
  altı çizili, ters, soluk; italik için ayrı font yok (normal çizilir).
- **İmleç:** Blok / çizgi / alt çizgi (programın istediği), panel odakta değilse içi boş blok; yanıp sönme yok.

## Input

- Panel odaktayken klavye terminale gider. Editör kısayollarından sadece `Ctrl+J` (alt alan), ``Ctrl+` `` (konsolu
  aç / kapat) ve `Ctrl+Shift+P` (komut paleti) çalışır; `TerminalView` bunları ayrılmış akor olarak almaz. Workspace
  önceki karede konsol odaktaysa diğer kısayollarını (`Ctrl+R`, `Ctrl+P`, `F5` ...) çalıştırmaz, tuş kabuğa gider.
- Yazılan karakterler (Türkçe ve IME dahil) ImGui'nin karakter kuyruğundan `vterm_keyboard_unichar`'a; özel tuşlar
  `vterm_keyboard_key`'e.
- **Ctrl+C:** seçim varsa kopyalar, yoksa programa kesme gönderir (Windows Terminal davranışı). `Ctrl+Shift+C` her
  zaman kopyalar; aşama 1'de seçim olmadığından hiçbir şey yapmaz (kopyalama alışkanlığı programı kesmesin).
- **Yapıştırma:** `Ctrl+V`, `Ctrl+Shift+V`, `Shift+Insert`, sağ tık menüsü (Paste, Restart). Satır sonları CR'ye çevrilir; program
  bracketed paste açtıysa sarılır (çok satırlı metin tek seferde gider, satır satır çalıştırılmaz).
- **Shift+Enter:** `ESC CR` gönderir; AI CLI'larının çok satırlı girişi böyle bekler (VS Code'da da aynı ayar).

## Aşama 2 ayrıntıları

- Geçmiş: libvterm'in ekrandan ittiği satırlar halkaya girer (varsayılan 10.000 satır). Alt ekrandaki (tam ekran)
  programlar geçmişe yazmaz.
- Tekerlek ve kaydırma çubuğu geçmişte gezer. Yeni çıktı gelince kullanıcı en alttaysa yapışık kalır; yukarıdaysa
  yeri korunur. Tuşa basınca en alta döner.
- Seçim hücre bazlıdır, satır sarmasını bilir (sarılmış satır kopyada birleşir), sondaki boşluklar atılır. Çift tık
  kelime, üç tık satır seçer.
- Sağ tık menüsü: Copy, Paste, Clear, Restart.

## Testler (`--label terminal`)

- `VtScreen`: bayt dizisi → hücreler (renk, kalın, geniş karakter, imleç), satırın geçmişe itilmesi, alt ekran.
- Tuş kodlama (`VtScreen`): Shift+Enter, Ctrl+harf, yön tuşları, bracketed paste sarması, CRLF dönüşümü.
- `Scrollback`: sınır, sarmalı satır birleştirme, seçimden metin.
- `PseudoConsole` duman testi: `cmd /c echo` çıktısı okunur, süreç çıkış kodu alınır, kapanış kilitlenmez.
- Çizim ve gerçek input testte yoktur; manuel kontrol listesi aşağıda.

## Manuel kontrol listesi

- Panel açılınca kabuk proje kökünde başlıyor; yeniden boyutlamada metin doğru sarılıyor.
- AI CLI'ı (ör. `claude`) açılıyor, renkler ve semboller doğru; izin sorusu klavyeyle cevaplanıyor.
- Shift+Enter yeni satır, Ctrl+C programı kesiyor, çok satırlı yapıştırma tek mesaj olarak gidiyor.
- Türkçe karakterler (ğ, ş, ı, İ) yazılıyor ve doğru görünüyor.
- Konsoldan `<engine> build`: derleme editörün Output'unda görünüyor, önizleme yenileniyor (D6).
- Editör kapanınca Görev Yöneticisi'nde kabuk / node süreci kalmıyor.
- `exit` → çıkış kodu görünüyor, Enter ile yeniden başlıyor.
- Aşama 2: geçmişte kaydırma, yeni çıktıda yapışma, seçim + kopyalama, çift tık kelime.

## Uygulama sırası

Her adım derlenebilir kalır; aşama sonlarında kullanıcı test eder ve commit'lenir.

1. **libvterm:** `ThirdParty/libvterm` (son sürüm, değiştirilmeden) + STATIC hedef. Sadece editörde kullanıldığı
   için `Notices.json`'a girmez; `ThirdParty/README.md` tarifine göre kaynak ve sürüm kaydı. Kontrol: sürüm paketinde
   kodlama tablolarının üretilmiş `.inc` dosyaları var mı; yoksa bir kez üretilir ve nasıl üretildiği yazılır.
2. **Çekirdek:** `PseudoConsole`, `VtScreen`, `TerminalSession` + testler.
3. **Görünüm:** `TerminalView` (çizim, input, font).
4. **Editör:** `EditorPanel::Console`, ayar kaydı, layout'lar, panel menüsü / komut paleti, `Ctrl+``.
   → Aşama 1 manuel test, commit.
5. **Aşama 2:** `Scrollback`, seçim, kopyalama, sağ tık menüsü + testler. → Manuel test, commit.
6. **Belgeler:** Kullanım `Docs/Editor/`'a, `Engine/CLAUDE.md` yapı tablosuna `Terminal/`; D18 "uygulandı".

## Riskler

- **ConPTY yeniden boyutlama:** ConPTY boyut değişince ekranı kendisi yeniden çizer; geçmişte yinelenen satırlar
  görülebilir (VS Code'un da bildiği sorun). Gerekirse yeniden boyutlama sürüklenirken geciktirilir.
- **Shift+Enter:** CLI'ın `ESC CR`'yi ConPTY üzerinden yeni satır olarak aldığı ilk adımda doğrulanır; olmazsa
  alternatif dizi denenir.
- **libvterm ve MSVC:** C99; Neovim Windows'ta derliyor. `/W4` kapsamı dışında tutulur.
- **Sembol glifleri:** Segoe UI Symbol her sembolü kapsamıyorsa eksik glif kutu olarak görünür; ikinci bir yedek font
  eklenebilir.
