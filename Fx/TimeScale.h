#pragma once

// Oyun zamanının akışı: hitstop (kısa donma) ve yavaş çekim. Game her frame'in gerçek
// süresini buradan geçirir; oyun ve efektler dönen süreyle ilerler.
class TimeScale {
public:
    // 'seconds' boyunca zaman durur.
    void Hitstop(float seconds);
    // 'seconds' gerçek saniye boyunca startScale'den endScale'e geçer, sonra normale döner.
    // İki ölçek aynıysa süre boyunca sabit yavaşlık; endScale verilmezse giderek normale döner.
    void SlowMo(float seconds, float startScale, float endScale = 1.0f);
    void Clear();
    // Gerçek süreyi oyun süresine çevirir; hitstop sürerken 0.
    [[nodiscard]] float Advance(float realSeconds);

private:
    float _hitstopSeconds = 0.0f; // Kalan donma süresi
    float _slowMoSeconds = 0.0f;  // Kalan yavaş çekim süresi
    float _slowMoDuration = 0.0f;
    float _slowMoStartScale = 1.0f;
    float _slowMoEndScale = 1.0f;
};
