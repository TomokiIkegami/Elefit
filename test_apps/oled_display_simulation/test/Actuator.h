#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <ESP32Servo.h>
#include <Arduino.h>
#include "Buzzer.h"

class Actuator {
private:
    Servo myservo;          // ESP32Servoライブラリのインスタンス
    uint8_t pin;            // サーボモーターを接続する物理ピン番号
    int currentAngle;       // ソフトウェア側で管理する現在の計算上のサーボ角度
    const int step = 2;     // ジョイスティック手動操作時に一回で動かすステップ角度（度）
    const int smoothDelay = 15; // スムーズ移動関数（moveSlowlyTo）での1度あたりのウエイト時間（ms）
    Buzzer* buzzer;         // BGM再生用Buzzerクラスへのポインタ（ポインタにすることで登録前は安全に空[nullptr]にできる）

public:
    // コンストラクタ：起動時の計算上の起点を180（Front）に設定し、Buzzerを初期化
    Actuator(uint8_t p) : pin(p), currentAngle(180), buzzer(nullptr) {}

    // 外部からBuzzerのインスタンス（参照）を登録するための関数
    void setBuzzer(Buzzer& b) { buzzer = &b; }

    // サーボモーターの初期化・ピン結合処理
    void setup() {
        myservo.setPeriodHertz(50); // 一般的なアナログサーボの制御周波数である50Hzに設定
        
        // 物理ピンに接続。制御パルス幅を500μs〜2400μsに制限してサーボの脱調や過負荷を防止
        myservo.attach(pin, 500, 2400);

        // 現在の角度(180)を初期値として書き込み
        myservo.write(currentAngle);
        delay(100); // 動作の安定化待ち

        // 起動時に180度（Front位置）までゆっくり移動して初期位置を保持
        moveSlowlyTo(180); 
    }

    // 指定したターゲット角度まで、1度ずつ刻みながら音楽を鳴らしてゆっくり移動する関数（自動シーケンス用）
    // targetAngle: 移動先の目標角度（0〜180度）
    void moveSlowlyTo(int targetAngle) {
        // サーボの動作限界を越えないように0〜180度の範囲に安全ガードをかける
        targetAngle = constrain(targetAngle, 0, 180);
        
        // 現在の計算角度が目標角度に達するまでループ処理
        while (currentAngle != targetAngle) {
            if (currentAngle < targetAngle) currentAngle++; // 目標より小さければ1度増やす
            else currentAngle--;                            // 目標より大きければ1度減らす
            
            myservo.write(currentAngle); // サーボに新しい角度を出力

            // 1度動かす間に挟む15msのウエイト。この間にBGM（音符）の更新処理を回して音途切れを防止する
            for (int i = 0; i < smoothDelay; i++) {
                if (buzzer != nullptr) {
                    buzzer->playDelfinoPlazaFull(); // BGMの進行・再生
                }
                delay(1);  // 1msの細切れウエイト
                yield();   // ESP32のバックグラウンド処理（Wi-Fi等）にCPUを明け渡してウォッチドッグリセットを防止
            }
        }
    }

    // 各定位置へスムーズに一発移動させるラッパー関数群
    void front()  { moveSlowlyTo(180); } // フロント（前進側）へ移動
    void back()   { moveSlowlyTo(0); }   // バック（後退側）へ移動
    void center() { moveSlowlyTo(90); }  // センター（中央）へ移動

    // 【ジョイスティック手動調整用】フロント方向へ指定ステップずつ進める関数
    void moveFront() {
        if (currentAngle < 180) {
            currentAngle += step; // 指定ステップ分（2度）増加
            if (currentAngle > 180) currentAngle = 180; // 上限ガード
            myservo.write(currentAngle);
        }
        // 手動操作中も音が途切れないようにBGMを1音符分進める
        if (buzzer != nullptr) buzzer->playDelfinoPlazaFull();
    }

    // 【ジョイスティック手動調整用】バック方向へ指定ステップずつ戻す関数
    void moveBack() {
        if (currentAngle > 0) {
            currentAngle -= step; // 指定ステップ分（2度）減少
            if (currentAngle < 0) currentAngle = 0; // 下限ガード
            myservo.write(currentAngle);
        }
        // 手動操作中も音が途切れないようにBGMを1音符分進める
        if (buzzer != nullptr) buzzer->playDelfinoPlazaFull();
    }

    // OLED画面の表示用などに、現在のサーボの内部角度（度）を返す関数
    int getCurrentAngle() { return currentAngle; }
};
#endif