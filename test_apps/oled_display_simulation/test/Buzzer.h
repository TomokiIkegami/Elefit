#ifndef BUZZER_H
#define BUZZER_H

#include <Arduino.h>
#include "pitches.h"

// テンポ設定 (各音符の長さ[ms]を定義。ゆったりとしたテンポを維持)
#define BEAT 900    // 1拍の長さ
#define HALF 450    // 半拍の長さ
#define LONG 1800   // 2拍の長さ

class Buzzer {
private:
    uint8_t pin;                    // ブザーを接続する物理ピン番号
    uint8_t channel;                // ESP32のLEDC（PWM）チャンネル番号
    int currentStep = 0;            // 現在再生している音符のインデックス
    int totalSteps = 0;             // 選択されたメロディの総音符数
    const int (*active_melody)[2];  // 現在再生中であるメロディ配列へのポインタ
    unsigned long nextNoteTime = 0; // 次の音符へ切り替える予定時刻（ms）
    unsigned long offTime = 0;      // 音符の間の「発音終了（消音）」予定時刻（ms）
    bool isMusicRunning = false;    // メロディ再生中のフラグ

    // 各フェーズごとのメロディ配列定義（{音程, 長さ} の構成）
    
    // 1. Washing: 透明感のある高音の響き
    static constexpr int mel_wash[][2] = {
        {NOTE_C5, LONG}, {NOTE_G5, LONG}, {NOTE_A5, LONG}, {NOTE_F5, LONG}
    };

    // 2. Loading: 軽やかに上昇する旋律
    static constexpr int mel_load[][2] = {
        {NOTE_C5, BEAT}, {NOTE_E5, BEAT}, {NOTE_G5, BEAT}, {NOTE_B5, BEAT},
        {NOTE_C6, LONG}, {0, BEAT} // 最後の {0, BEAT} は1拍分の「休符」
    };

    // 3. Collecting: キラキラとした、間隔の広い旋律
    static constexpr int mel_coll[][2] = {
        {NOTE_G5, BEAT}, {NOTE_E5, BEAT}, {NOTE_C5, LONG},
        {0, BEAT}, // 1拍分の「休符」
        {NOTE_A4, BEAT}, {NOTE_B4, BEAT}, {NOTE_C5, LONG}
    };

    // 4. Discharge: 落ち着いた、澄んだ下降音
    static constexpr int mel_disc[][2] = {
        {NOTE_C5, LONG}, {NOTE_G4, LONG}, {NOTE_C4, 3000} 
    };

    // 終了音（ResetやClose選択時）: 軽快に結ぶ2音（このメロディのみdelayでブロッキング再生される）
    static constexpr int mel_close[][2] = {
        {NOTE_G5, BEAT}, {NOTE_C6, 1500}
    };

public:
    // コンストラクタ：ピン番号とLEDCチャンネル（デフォルトは10）を登録
    Buzzer(uint8_t p, uint8_t ch = 10) : pin(p), channel(ch) {}

    // 初期化関数：ESP32専用のLEDC割り当て（ピン、周波数2000Hz、8bit解像度、チャンネル）
    void setup() { ledcAttachChannel(pin, 2000, 8, channel); }

    // 各フェーズのBGMセット＆再生開始ラッパー
    void startWashing()    { active_melody = mel_wash; totalSteps = 4; startMusic(); }
    void startLoading()    { active_melody = mel_load; totalSteps = 6; startMusic(); }
    void startCollecting() { active_melody = mel_coll; totalSteps = 7; startMusic(); }
    void startDischarge()  { active_melody = mel_disc; totalSteps = 3; startMusic(); }

    // メロディの再生位置をリセットし、タイマーを始動させる内部関数
    void startMusic() {
        currentStep = 0;
        nextNoteTime = millis(); // 待機なしで即座に1音目を鳴らすための設定
        isMusicRunning = true;
    }

    // 再生フラグを下げ、出力を0HzにしてBGMを停止させる関数
    void stopMusic() {
        isMusicRunning = false;
        ledcWriteTone(pin, 0); // 音を消す
    }

    // 終了音（Reset時など）を delay() を使って確実に最後まで鳴らしきる関数
    void playCloseSound() {
        stopMusic(); // 通常BGMを停止
        for(int i = 0; i < 2; i++) {
            ledcWriteTone(pin, mel_close[i][0]); // 指定の周波数で発音
            delay(mel_close[i][1]);              // 指定時間ウェイト（ブロッキング）
        }
        ledcWriteTone(pin, 0); // 最後に消音
    }

    // 【重要】シーケンスループの周回ごとに毎回呼び出す、ノンブロッキングBGM再生関数
    // delay() を一切使わないタイムスライス管理により、ポンプやUIを止めずに音楽を並行再生する
    void playDelfinoPlazaFull() {
        unsigned long now = millis();
        
        // 音符の「切れ目」を作る消音（スタッカート）処理
        // 高音域が耳障り（うるさく）ならないよう、音符の長さの前半50%だけ発音し、後半50%で消音する
        if (offTime > 0 && now >= offTime) {
            ledcWriteTone(pin, 0); // 後半の消音を実行
            offTime = 0;           // 消音タイマーをリセット
        }
        
        if (!isMusicRunning) return; // BGMフラグが立っていなければ処理を抜ける
        
        // 次の音符を鳴らすタイミング（予定時刻）に達したか判定
        if (now >= nextNoteTime) {
            int freq = active_melody[currentStep][0];     // 現在のステップの周波数
            int duration = active_melody[currentStep][1]; // 現在のステップの音符の長さ

            if (freq > 0) { // 休符（0）でなければ発音
                ledcWriteTone(pin, freq);
                offTime = now + (duration * 50 / 100); // 切れ目用の消音時刻（長さの50%の位置）を計算
            }
            
            if (nextNoteTime == 0) nextNoteTime = now;
            nextNoteTime += (unsigned long)duration; // 次の音符の再生開始予定時刻を更新
            
            // 次のステップへ進める。配列の最後まで達したら `totalSteps` で割った余りにより自動で0（最初）に戻る（ループ再生）
            currentStep = (currentStep + 1) % totalSteps;
        }
    }

    // 手動操作のカーソル移動（ピッ）や確定（ピピッ）など、短い単発音を鳴らす関数
    // frequency: 鳴らしたい音の周波数
    // duration : 鳴らす長さ[ms]（この関数内ではdelayを使用）
    void play(int frequency, int duration) {
        ledcWriteTone(pin, frequency); // 発音
        delay(duration);               // 鳴らす時間分ウェイト
        ledcWriteTone(pin, 0);         // 消音
        
        // 通常BGMが裏で動いていた場合、単発音が鳴ったことによってBGMの再生スケジュールがズレて音が伸びてしまうのを防ぐため、
        // 単発音が終わった「今の時間」をBGMの次の音符スケジュール起点として再同期するガード処理
        if (isMusicRunning) nextNoteTime = millis();
    }
};

#endif