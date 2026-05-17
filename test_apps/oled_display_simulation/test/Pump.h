#ifndef PUMP_H
#define PUMP_H
#include <Arduino.h>

class Pump {
private:
    // 各種制御ピンを保持する内部変数
    // pA~pC: 独立した3つのDBA用ポンプ (Pump1~3)
    // rF/rR: 12ch右側ポンプの正転(Forward) / 逆転(Reverse)
    // lF/lR: 12ch左側ポンプの正転(Forward) / 逆転(Reverse)
    uint8_t pA, pB, pC, rF, rR, lF, lR;

public:
    // コンストラクタ：メインプログラム側で指定されたピン番号を内部変数にマッピング
    Pump(uint8_t a, uint8_t b, uint8_t c, uint8_t rf, uint8_t rr, uint8_t lf, uint8_t lr) 
        : pA(a), pB(b), pC(c), rF(rf), rR(rr), lF(lf), lR(lr) {}

    // 初期化関数：すべてのピンを出力(OUTPUT)モードに設定し、初期状態としてすべて停止させる
    void setup() {
        pinMode(pA, OUTPUT); pinMode(pB, OUTPUT); pinMode(pC, OUTPUT);
        pinMode(rF, OUTPUT); pinMode(rR, OUTPUT); pinMode(lF, OUTPUT); pinMode(lR, OUTPUT);
        stopAll(); // 起動時にすべてのポンプを確実にOFFにする
    }

    // 指定したIDのDBAポンプ（1〜3）を個別にONにする関数
    void onDBA(int id) {
        if(id == 1)      digitalWrite(pA, HIGH); // Pump1 を駆動
        else if(id == 2) digitalWrite(pB, HIGH); // Pump2 を駆動
        else if(id == 3) digitalWrite(pC, HIGH); // Pump3 を駆動
    }
    
    // すべてのDBAポンプ（1〜3）を一括でOFFにする関数
    void offDBA() { 
        digitalWrite(pA, LOW); 
        digitalWrite(pB, LOW); 
        digitalWrite(pC, LOW); 
    }

    // 12chポンプ（左右2基）を、指定した速度と回転方向で駆動する関数
    // speed: 0~100 [%] (実機用アナログ制御時に使用)
    // rev  : trueで逆転(背面への回収など)、falseで正転
    void on12ch(int speed, bool rev) {
        if(!rev) {
            // 【正転制御 (Forward)】
            // シミュレーション用（Wokwi上のLED等のデジタル確認用）
            digitalWrite(rR, LOW);  digitalWrite(lR, LOW);
            digitalWrite(rF, HIGH); digitalWrite(lF, HIGH);

            // --- 実機モータードライバ用（実機へ書き込む際は以下のコメントを解除） ---
            // analogWrite(rF, speed * 2.55); // 0~100% の速度を 0~255 のPWM値に変換して出力
            // analogWrite(lF, speed * 2.55);
            // digitalWrite(rR, LOW);          // 逆転側ピンは確実にLOWにしてショートを防止
            // digitalWrite(lR, LOW);
        } else {
            // 【逆転制御 (Reverse)】
            // シミュレーション用（Wokwi上のLED等のデジタル確認用）
            digitalWrite(rF, LOW);  digitalWrite(lF, LOW);
            digitalWrite(rR, HIGH); digitalWrite(lR, HIGH);

            // --- 実機モータードライバ用（実機へ書き込む際は以下のコメントを解除） ---
            // digitalWrite(rF, LOW);          // 正転側ピンは確実にLOWにしてショートを防止
            // digitalWrite(lF, LOW);
            // analogWrite(rR, speed * 2.55); // 逆転側ピンにPWM出力を行って速度制御
            // analogWrite(lR, speed * 2.55);
        }
    }

    // 12chポンプのみを即座に停止させる関数
    void stop12ch() {
        digitalWrite(rF, LOW); digitalWrite(rR, LOW);
        digitalWrite(lF, LOW); digitalWrite(lR, LOW);
    }

    // システムに繋がっているすべてのポンプ（DBAおよび12ch）を一括で即座に完全停止させる緊急用関数
    void stopAll() { 
        offDBA(); 
        stop12ch(); 
    }
};
#endif