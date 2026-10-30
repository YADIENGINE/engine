# ThirdParty

Engine'in bütün dış bağımlılıkları buradadır; derleme internet istemez ve kopyalanan bir engine kendi başına
eksiksizdir. Kaynaklar **değiştirilmeden** durur (git satır sonlarını da değiştirmez: `.gitattributes`). Sadece
derleme için gerekmeyen klasörler (örnekler, belgeler, testler) alınmaz.

| Klasör | Sürüm | Kaynak | Kim kullanır | Oyuna girer |
| --- | --- | --- | --- | --- |
| `raylib/` | 5.5 | https://github.com/raysan5/raylib/archive/refs/tags/5.5.tar.gz | `Engine` | Evet |
| `nlohmann_json/` | 3.12.0 | https://github.com/nlohmann/json/releases/download/v3.12.0/json.tar.xz | `EngineJsonIO` | Evet |
| `Monocypher/` | 4.0.3 | https://monocypher.org | `EngineJsonIO` (private) | Evet |
| `imgui/` | 1.92.7-docking | https://github.com/ocornut/imgui/archive/refs/tags/v1.92.7-docking.tar.gz | Editör | Hayır |
| `rlImGui/` | 118221c | https://github.com/raylib-extras/rlImGui/archive/118221c8a532501a3c2c236b62950877cbefc2b4.tar.gz | Editör | Hayır |
| `Catch2/` | 3.16.0 | https://github.com/catchorg/Catch2/archive/refs/tags/v3.16.0.tar.gz | Testler (`engine_use_catch2()`) | Hayır |

Alınan parçalar: raylib `CMakeLists.txt`, `CMakeOptions.txt`, `cmake/`, `src/`, `raylib.pc.in`, lisans;
nlohmann_json release paketinin tamamı; imgui kökteki `.cpp` / `.h` ve lisans; rlImGui kökteki kaynaklar,
`extras/` ve lisans; Catch2 `CMakeLists.txt`, `CMake/`, `src/`, `extras/` ve lisans.

## Lisans bildirimi

Oyuna giren bileşenler (tabloda "Evet") her oyunun `ThirdPartyNotices.txt`'inin engine kısmına girer:

- `Notices.json`: bileşen listesi. `text` bildirim metni, `pin` kaynakta sürümü kanıtlayan dosya ve metin
  (`{version}` sürümle değişir), `license` metnin aynen içermesi gereken lisans dosyası. Yollar bu klasöre göredir.
- `Notices/<bileşen>.txt`: ilk satır `<ad> <sürüm> (<lisans>)`, sonra `Source:`, telif satırları ve lisans metni;
  bileşenin içinde gelen alt kütüphaneler aynı dosyada kendi başlıklarıyla.
- `notices.py`: denetim ve metin üretimi; `ReleaseSystem/release.py` paketlerken kullanır.

## Bağımlılık eklemek ya da yükseltmek

1. Kaynağı indir, tablodaki parçaları `ThirdParty/<bileşen>/`'e koy (eskisini tamamen sil, dosya karıştırma).
2. CMake'te bağla (`add_subdirectory` ya da kaynak listesi); `FetchContent` kullanılmaz.
3. Bu README'deki tabloyu güncelle.
4. Oyuna giriyorsa aynı committe `Notices/<bileşen>.txt` ve `Notices.json` (sürüm, pin, lisans).
5. Engine kökünde `python -m unittest discover -s ThirdParty/Tests`.
