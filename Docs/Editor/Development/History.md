# Editor design history

Ekim 2026 yapısında genel editör ile proje içeriği ayrıdır.
Katalog/worker süreçleri UI çökmesi ile oyun kaynaklarının ömrünü ayırır.
Üretim Game/View çizimleri önizlemede tekrar kullanılır.

Sunum saati, View başına bağlama yerine Engine DrawTime ve Game::DrawSeconds üzerinden
sağlanır. Böylece Pause/Step her View için ayrı bağlantı gerektirmez.
PreviewSession kontrol ve kare aktarımını Game'den ayırır; varsayılan PreviewGame veya
tüketicinin gerçek Game adaptörü aynı runtime sözleşmesini kullanır.

Genel belgeler Docs/Editor altında, projeye özel katalog/geliştirme geçmişi
tüketici projenin kendi belgelerinde tutulur.
