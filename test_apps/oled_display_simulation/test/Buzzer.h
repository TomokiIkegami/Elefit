#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "pitches.h"

// テンポ設定 (ゆったりとしたテンポを維持)
#define BEAT 900    
#define HALF 450    
#define LONG 1800   

class Buzzer {
private:
    uint8_t pin;
    uint8_t channel;
    int currentStep = 0;
    int totalSteps = 0;
    const int (*active_melody)[2];
    unsigned long nextNoteTime = 0;
    unsigned long offTime = 0;
    bool isMusicRunning = false;

    // 1. Washing: 透明感のある高音の響き
    static constexpr int mel_wash[][2] = {
        {NOTE_C5, LONG}, {NOTE_G5, LONG}, {NOTE_A5, LONG}, {NOTE_F5, LONG}
    };

    // 2. Loading: 軽やかに上昇する旋律
    static constexpr int mel_load[][2] = {
        {NOTE_C5, BEAT}, {NOTE_E5, BEAT}, {NOTE_G5, BEAT}, {NOTE_B5, BEAT},
        {NOTE_C6, LONG}, {0, BEAT} 
    };

    // 3. Collecting: キラキラとした、間隔の広い旋律
    static constexpr int mel_coll[][2] = {
        {NOTE_G5, BEAT}, {NOTE_E5, BEAT}, {NOTE_C5, LONG},
        {0, BEAT},
        {NOTE_A4, BEAT}, {NOTE_B4, BEAT}, {NOTE_C5, LONG}
    };

    // 4. Discharge: 落ち着いた、澄んだ下降音
    static constexpr int mel_disc[][2] = {
        {NOTE_C5, LONG}, {NOTE_G4, LONG}, {NOTE_C4, 3000} 
    };

    // 終了音: 軽快に結ぶ2音
    static constexpr int mel_close[][2] = {
        {NOTE_G5, BEAT}, {NOTE_C6, 1500}
    };

public:
    Buzzer(uint8_t p, uint8_t ch = 10) : pin(p), channel(ch) {}

    void setup() { ledcAttachChannel(pin, 2000, 8, channel); }

    void startWashing()    { active_melody = mel_wash; totalSteps = 4; startMusic(); }
    void startLoading()    { active_melody = mel_load; totalSteps = 6; startMusic(); }
    void startCollecting() { active_melody = mel_coll; totalSteps = 7; startMusic(); }
    void startDischarge()  { active_melody = mel_disc; totalSteps = 3; startMusic(); }

    void startMusic() {
        currentStep = 0;
        nextNoteTime = millis();
        isMusicRunning = true;
    }

    void stopMusic() {
        isMusicRunning = false;
        ledcWriteTone(pin, 0);
    }

    void playCloseSound() {
        stopMusic();
        for(int i = 0; i < 2; i++) {
            ledcWriteTone(pin, mel_close[i][0]);
            delay(mel_close[i][1]);
        }
        ledcWriteTone(pin, 0);
    }

    void playDelfinoPlazaFull() {
        unsigned long now = millis();
        // 50%の消音時間を維持して、高音でも「うるさく」ならないよう配慮
        if (offTime > 0 && now >= offTime) {
            ledcWriteTone(pin, 0);
            offTime = 0;
        }
        if (!isMusicRunning) return;
        if (now >= nextNoteTime) {
            int freq = active_melody[currentStep][0];
            int duration = active_melody[currentStep][1];
            if (freq > 0) {
                ledcWriteTone(pin, freq);
                offTime = now + (duration * 50 / 100);
            }
            if (nextNoteTime == 0) nextNoteTime = now;
            nextNoteTime += (unsigned long)duration;
            currentStep = (currentStep + 1) % totalSteps;
        }
    }

    void play(int frequency, int duration) {
        ledcWriteTone(pin, frequency);
        delay(duration);
        ledcWriteTone(pin, 0);
        if (isMusicRunning) nextNoteTime = millis();
    }
};

#endif