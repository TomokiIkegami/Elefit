#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "Pump.h"
#include "Actuator.h"
#include "UI.h"
#include "Buzzer.h"
#include "pitches.h"
#include "Communication.h" 

#define SIM_SPEED_SCALE 20 // シミュレーション速度の倍率設定 (数値を大きくすると工程が早く進む)
#define JOY_SW_PIN 25      // 長押し割り込み監視用のジョイスティックボタン物理ピン

class SequenceController {
private:
    Pump& pumps;
    Actuator& servo;
    UI& ui;
    Buzzer& buzzer;
    Communication& comm; 

    // 長押し判定用タイマー変数
    unsigned long seqButtonStartTime = 0;
    bool isSeqButtonPressing = false;

    // ポンプの基本駆動速度（パーセント駆動用）
    const int speed1 = 100; 
    const int speed2 = 100;
    const int currentSpeed = speed1; 
    const int pwmWashSpeed = 80; // 洗浄フェーズ 1-3 特殊パルス駆動用の速度

    // 各個別工程の実行秒数設定 (実機の規定秒数)
    const unsigned long t_11 = 18, t_12 = 45, t_13 = 600, t_14 = 30; // Washing用
    const unsigned long t_21 = 19, t_22 = 45, t_23 = 300, t_24 = 250, t_25 = 4, t_26 = 60; // Loading用
    const unsigned long t_31 = 15, t_32 = 45, t_33 = 5,   t_34 = 60,  t_35 = 5, t_36 = 100; // Collecting用
    const unsigned long t_41 = 7,  t_42 = 280; // Discharge用

    // 各フェーズごとの総実行秒数の自動計算
    const unsigned long total_wash = t_11 + t_12 + t_13 + t_14;
    const unsigned long total_load = t_21 + t_22 + t_23 + t_24 + (3 * (t_25 + t_26)); // ループ3回分を加算
    const unsigned long total_coll = t_31 + t_32 + t_33 + t_34 + t_35 + t_36;
    const unsigned long total_disc = t_41 + t_42;

    // 【長押し判定】シーケンス実行中のループ内からいつでも2秒長押しで強制リセットさせる関数
    void checkHardReset() {
        if (!digitalRead(JOY_SW_PIN)) { // ボタン押し込み検知
            if (!isSeqButtonPressing) {
                seqButtonStartTime = millis(); // 押し始めの時刻を記録
                isSeqButtonPressing = true;
            } else if (millis() - seqButtonStartTime >= 2000) { // 2秒経過時
                pumps.stopAll(); // 安全のため、即座にすべてのポンプを緊急停止
                ui.drawSystemMessage("RESET", true);
                buzzer.play(NOTE_ERROR, 500); // 警告音(NOTE_C3)を500ms鳴らす
                delay(800);
                void(* resetFunc) (void) = 0;
                resetFunc(); // ソフトウェア再起動を実行
            }
        } else {
            isSeqButtonPressing = false; // ボタンが離されたらタイマーリセット
        }
    }

    // 【定期送信】シーケンス実行中のループ内からProcessingへ進捗状況を500msおきに定期報告する関数
    void sendPeriodicStatus() {
        static unsigned long lastSeqStatusTime = 0;
        if (millis() - lastSeqStatusTime >= 500) { 
            lastSeqStatusTime = millis();
            int w=0, l=0, c=0, d=0;
            // 現在実行中のアクティブフェーズのみ進捗パーセントを割り振る
            if (strcmp(activePhase, "washing") == 0) w = currentPercent;
            else if (strcmp(activePhase, "loading") == 0) l = currentPercent;
            else if (strcmp(activePhase, "collecting") == 0) c = currentPercent;
            else if (strcmp(activePhase, "discharge") == 0) d = currentPercent;
            
            // Communicationクラス経由でデータを送信（シリアルモニタにも同時にデバッグ表記が出る）
            comm.sendStatus(remainingSec, w, l, c, d);
        }
    }

    // 各個別工程のウェイト時間を管理し、進行に合わせてOLEDや通信をアップデートする基幹関数
    // stepSec: このステップの規定秒数, totalPhaseSec: フェーズ全体の総秒数, elapsedPhaseSec: フェーズ内累積経過秒数
    void waitInPhase(unsigned long stepSec, unsigned long totalPhaseSec, unsigned long& elapsedPhaseSec, const char* label, const char* debugLog) {
        Serial.print("[DEBUG] "); Serial.println(debugLog); // 現在のステップ（例："1-1"）をシリアルログに出力
        unsigned long startWait = millis();
        // シミュレーションスケール倍率（SIM_SPEED_SCALE）を適用して実際の待ち時間をミリ秒に換算
        unsigned long targetWait = (unsigned long)((stepSec * 1000) / SIM_SPEED_SCALE);
        
        // 換算した目標時間に達するまでループして時間を消費させる
        while (millis() - startWait < targetWait) {
            unsigned long stepElapsed = millis() - startWait; // このステップ内で経過した実際のミリ秒
            // フェーズ全体の総経過時間（ミリ秒）をシミュレーション速度に逆換算して算出
            float currentTotalMs = (elapsedPhaseSec * 1000) + (stepElapsed * SIM_SPEED_SCALE);
            int percent = (int)(currentTotalMs / (totalPhaseSec * 10)); // パーセント計算
            if (percent > 100) percent = 100;

            currentPercent = percent;
            // 全体総秒数から逆換算した実経過時間を引き、残りの推定秒数を算出
            remainingSec = (int)(((totalPhaseSec - elapsedPhaseSec) * 1000 - stepElapsed * SIM_SPEED_SCALE) / 1000);
            if (remainingSec < 0) remainingSec = 0;

            ui.drawProgressBar(label, percent); // OLEDプログレスバーを更新
            sendPeriodicStatus();              // Processingへ進捗を定期通信
            checkHardReset();                  // ボタン2秒長押しを常時監視

            buzzer.playDelfinoPlazaFull();     // 音切れを起こさないようにBGMを1音符分進める
            delay(1);                          // 最小単位のミリ秒ウェイト
            yield();                           // WDT（ウォッチドッグ）リセット防止用バックグラウンド処理明け渡し
        }
        elapsedPhaseSec += stepSec; // ステップ完了後、フェーズ全体の経過秒数にこのステップ分を加算
    }

public:
    int currentPercent = 0;           // メイン側から参照される現在の進捗率（0〜100%）
    int remainingSec = 0;              // メイン側から参照される現在の残り秒数
    const char* activePhase = "IDLE"; // 現在の動作状況文字列（"washing", "IDLE" など）

    // コンストラクタ
    SequenceController(Pump& p, Actuator& s, UI& u, Buzzer& b, Communication& c) 
        : pumps(p), servo(s), ui(u), buzzer(b), comm(c) {}

    // ==========================================
    // 1. 洗浄シーケンス (Washing)
    // ==========================================
    void washing() {
        activePhase = "washing";
        buzzer.startWashing(); 
        servo.front(); // サーボモーターを洗浄位置（Front）へ移動
        unsigned long elapsed = 0; 
        
        // 工程1-1: DBA1をONにして規定時間ウェイト
        pumps.onDBA(1); waitInPhase(t_11, total_wash, elapsed, "Washing...", "1-1"); pumps.offDBA();
        
        // 工程1-2: 12ch正転をONにして規定時間ウェイト
        pumps.on12ch(currentSpeed, false); 
        waitInPhase(t_12, total_wash, elapsed, "Washing...", "1-2"); 
        pumps.stop12ch();
        
        // 工程1-3: ポンプを細かくON/OFF駆動させるパルス洗浄特殊ブロック
        Serial.println("[DEBUG] 1-3");
        unsigned long pwmStart = millis();
        unsigned long pwmTarget = (t_13 * 1000) / SIM_SPEED_SCALE;
        while (millis() - pwmStart < pwmTarget) {
            int percent = (int)(((elapsed * 1000) + (millis() - pwmStart) * SIM_SPEED_SCALE) / (total_wash * 10));
            currentPercent = percent;
            
            // 1-3内でのリアルタイム残り秒数計算
            remainingSec = (int)(((total_wash - elapsed) * 1000 - (millis() - pwmStart) * SIM_SPEED_SCALE) / 1000);
            if (remainingSec < 0) remainingSec = 0;

            ui.drawProgressBar("Washing...", percent > 100 ? 100 : percent);
            sendPeriodicStatus();
            checkHardReset();

            // パルスONフェーズ: 速度80で385ms分駆動 (BGM再生と長押し・通信監視を兼ねる)
            pumps.on12ch(pwmWashSpeed, false); 
            unsigned long t = millis();
            while(millis() - t < (385 / SIM_SPEED_SCALE)) { 
                buzzer.playDelfinoPlazaFull(); 
                sendPeriodicStatus(); 
                checkHardReset();
                delay(1); 
            }
            // パルスOFFフェーズ: ポンプを止めて625ms分静止 (BGM再生と長押し・通信監視を兼ねる)
            pumps.stop12ch();    
            t = millis();
            while(millis() - t < (625 / SIM_SPEED_SCALE)) { 
                buzzer.playDelfinoPlazaFull(); 
                sendPeriodicStatus(); 
                checkHardReset();
                delay(1); 
            }
            yield();
        }
        elapsed += t_13;
        
        // 工程1-4: 12ch正転をONにして規定時間ウェイト
        pumps.on12ch(currentSpeed, false); waitInPhase(t_14, total_wash, elapsed, "Washing...", "1-4"); pumps.stop12ch();
        
        ui.drawSystemMessage("WASH DONE", true);
        activePhase = "IDLE";
        comm.clearBuffer(); // 洗浄中にProcessing側で連打された不要なボタン入力をすべて破棄（誤作動防止）
    }

    // ==========================================
    // 2. 充填シーケンス (Loading)
    // ==========================================
    void loading() {
        activePhase = "loading";
        buzzer.startLoading(); 
        servo.front();
        unsigned long elapsed = 0;
        
        pumps.onDBA(2); waitInPhase(t_21, total_load, elapsed, "Loading...", "2-1"); pumps.offDBA();
        pumps.on12ch(currentSpeed, false); waitInPhase(t_22, total_load, elapsed, "Loading...", "2-2"); pumps.stop12ch();
        waitInPhase(t_23, total_load, elapsed, "Loading...", "2-3"); 
        pumps.on12ch(currentSpeed, false); waitInPhase(t_24, total_load, elapsed, "Loading...", "2-4"); pumps.stop12ch();
        
        // 工程2-5 & 2-6: ループによる段階充填（3回繰り返し）
        for(int i=0; i<3; i++) {
            pumps.onDBA(3); waitInPhase(t_25, total_load, elapsed, "Loading...", "2-5"); pumps.offDBA();
            pumps.on12ch(currentSpeed, false); waitInPhase(t_26, total_load, elapsed, "Loading...", "2-6"); pumps.stop12ch();
        }
        
        ui.drawSystemMessage("LOAD DONE", true);
        activePhase = "IDLE";
        comm.clearBuffer(); // 動作中に溜まった不要シリアルバッファをクリア
    }

    // ==========================================
    // 3. 回収シーケンス (Collecting)
    // ==========================================
    void collecting() {
        activePhase = "collecting";
        buzzer.startCollecting(); 
        unsigned long elapsed = 0;
        
        pumps.onDBA(3); waitInPhase(t_31, total_coll, elapsed, "Collecting...", "3-1"); pumps.offDBA();
        pumps.on12ch(currentSpeed, false); waitInPhase(t_32, total_coll, elapsed, "Collecting...", "3-2"); pumps.stop12ch();
        
        // 工程3-3: 12ch逆転駆動 (背面へ送る)
        pumps.on12ch(currentSpeed, true); 
        waitInPhase(t_33, total_coll, elapsed, "Collecting...", "3-3"); pumps.stop12ch();
        
        servo.back(); // サーボを回収用の後退位置（Back）へ移動
        pumps.on12ch(currentSpeed, false); waitInPhase(t_34, total_coll, elapsed, "Collecting...", "3-4"); pumps.stop12ch();
        
        servo.front(); // サーボを前方（Front）へ戻す
        pumps.on12ch(currentSpeed, true); waitInPhase(t_35, total_coll, elapsed, "Collecting...", "3-5"); pumps.stop12ch();
        pumps.on12ch(currentSpeed, false); waitInPhase(t_36, total_coll, elapsed, "Collecting...", "3-6"); pumps.stop12ch();
        
        ui.drawSystemMessage("COLL DONE", true);
        activePhase = "IDLE";
        comm.clearBuffer(); // 動作中に溜まった不要シリアルバッファをクリア
    }

    // ==========================================
    // 4. 排出シーケンス (Discharge)
    // ==========================================
    void discharge() {
        activePhase = "discharge";
        buzzer.startDischarge(); 
        servo.front();
        unsigned long elapsed = 0;
        
        // 工程4-1: DBA1〜3のすべての個別ポンプを全開にして一斉排出
        pumps.onDBA(1); pumps.onDBA(2); pumps.onDBA(3);
        waitInPhase(t_41, total_disc, elapsed, "Discharge...", "4-1"); pumps.offDBA();
        
        // 工程4-2: 12ch正転による最終排出仕上げ駆動
        pumps.on12ch(currentSpeed, false); waitInPhase(t_42, total_disc, elapsed, "Discharge...", "4-2"); pumps.stop12ch();
        
        ui.drawSystemMessage("DRAIN DONE", true);
        activePhase = "IDLE";
        comm.clearBuffer(); // 動作中に溜まった不要シリアルバッファをクリア
    }

    // ==========================================
    // 5. 連続複合シーケンス関数群
    // ==========================================
    // 洗浄 ＞ 充填 ＞ 回収を連続実行。途中でPC側ボタンが連打されても最後のclearBufferで完全防御する
    void allPhase() { washing(); loading(); collecting(); ui.drawSystemMessage("ALL DONE", true); comm.clearBuffer(); }
    
    // 充填 ＞ 回収を連続実行。同様に終了時にシリアル先行入力をゴミ箱へ捨てる
    void loading_collecting() { loading(); collecting(); ui.drawSystemMessage("L+C DONE", true); comm.clearBuffer(); }
};
#endif