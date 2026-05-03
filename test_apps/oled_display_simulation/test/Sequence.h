#ifndef SEQUENCE_H
#define SEQUENCE_H
#include "Pump.h"
#include "Actuator.h"
#include "UI.h"

// --- シミュレーション短縮設定 ---
// 1 = 実機時間, 10 = 10倍速, 100 = 100倍速
#define SIM_SPEED_SCALE 10 

// 待機時間を計算するマクロ（msに変換しつつ、倍率で割る）
#define WAIT(sec) delay((unsigned long)((sec * 1000) / SIM_SPEED_SCALE))

class SequenceController {
private:
    Pump& pumps;
    Actuator& servo;
    UI& ui;

    // --- 各フェーズの実行時間 [秒] ---
    unsigned long t_11 = 18, t_12 = 45, t_13 = 600, t_14 = 30; // Washing
    unsigned long t_21 = 19, t_22 = 45, t_23 = 300, t_24 = 250, t_25 = 4, t_26 = 60; // Loading
    unsigned long t_31 = 15, t_32 = 45, t_33 = 5, t_34 = 60, t_35 = 5, t_36 = 100; // Collecting
    unsigned long t_41 = 7,  t_42 = 280; // Discharge

    // 12chポンプの「脈動洗浄」を再現する関数
    void pwm_pump_12ch(unsigned long duration_sec) {
        unsigned long start = millis();
        // ここもスケールを適用してループ時間を短縮
        unsigned long total_ms = (duration_sec * 1000) / SIM_SPEED_SCALE;
        
        while (millis() - start <= total_ms) {
            pumps.on12ch(true); 
            delay(385 / SIM_SPEED_SCALE); // ON時間も短縮
            pumps.stop12ch();    
            delay(625 / SIM_SPEED_SCALE); // OFF時間も短縮
        }
    }

public:
    SequenceController(Pump& p, Actuator& s, UI& u) : pumps(p), servo(s), ui(u) {}

    void washing() {
        ui.drawStatus("RUN: Washing");
        servo.front();
        /* 1-1 */
        Serial.print("1-1\n");
        pumps.onDBA(1); WAIT(t_11); pumps.offDBA();
        /* 1-2 */
        Serial.print("1-2\n");
        pumps.on12ch(true); WAIT(t_12); pumps.stop12ch();
        /* 1-3 */
        Serial.print("1-3\n");
        pwm_pump_12ch(t_13);
        /* 1-4 */
        Serial.print("1-4\n");
        pumps.on12ch(true); WAIT(t_14); pumps.stop12ch();
    }

    void loading() {
        ui.drawStatus("RUN: Loading");
        servo.front();
        /* 2-1 */
        Serial.print("2-1\n");
        pumps.onDBA(2); WAIT(t_21); pumps.offDBA();
        /* 2-2 */
        Serial.print("2-2\n");
        pumps.on12ch(true); WAIT(t_22); pumps.stop12ch();
        /* 2-3 */
        Serial.print("2-3\n");
        WAIT(t_23); // 浸透待機
        /* 2-4 */
        Serial.print("2-4\n");
        pumps.on12ch(true); WAIT(t_24); pumps.stop12ch();
        /* 2-5 */ /* 2-6 */ 
        for(int i=0; i<3; i++) {
            Serial.printf("2-5-%d,2-6-%d\n", i+1, i+1);
            pumps.onDBA(3); WAIT(t_25); pumps.offDBA();
            pumps.on12ch(true); WAIT(t_26); pumps.stop12ch();
        }
    }

    void collecting() {
        ui.drawStatus("RUN: Collecting");
        /* 3-1 */
        Serial.print("3-1\n");
        pumps.onDBA(3); WAIT(t_31); pumps.offDBA();
        /* 3-2 */
        Serial.print("3-2\n");
        pumps.on12ch(true); WAIT(t_32); pumps.stop12ch();
        /* 3-3 */
        Serial.print("3-3\n");
        pumps.on12ch(false);  WAIT(t_33); pumps.stop12ch();

        servo.back();

        /* 3-4 */
        Serial.print("3-4\n");
        pumps.on12ch(true); WAIT(t_34); pumps.stop12ch();

        servo.front();

        /* 3-5 */
        Serial.print("3-5\n");
        pumps.on12ch(false);  WAIT(t_35); pumps.stop12ch();
        /* 3-6 */
        Serial.print("3-6\n");
        pumps.on12ch(true); WAIT(t_36); pumps.stop12ch();
        
    }

    void discharge() {
        ui.drawStatus("RUN: Discharge");
        servo.front();
        /* 4-1 */
        Serial.print("4-1\n");
        pumps.onDBA(1); pumps.onDBA(2); pumps.onDBA(3);
        WAIT(t_41); pumps.offDBA();

        /* 4-2 */
        Serial.print("4-1\n");
        pumps.on12ch(true); WAIT(t_42); pumps.stop12ch();
        
    }

    void allPhase() { washing(); loading(); collecting(); ui.drawStatus("ALL DONE"); }
    void loading_collecting() { loading(); collecting(); ui.drawStatus("L+C DONE"); }
};
#endif