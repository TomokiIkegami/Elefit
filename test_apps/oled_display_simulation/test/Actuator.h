#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <ESP32Servo.h>
#include <Arduino.h>
#include "Buzzer.h"

class Actuator {
private:
    Servo myservo;
    uint8_t pin;
    int currentAngle;
    const int step = 2;
    const int smoothDelay = 15;
    Buzzer* buzzer;

public:
    // 起動時の計算上の起点を180に設定
    Actuator(uint8_t p) : pin(p), currentAngle(180), buzzer(nullptr) {}

    void setBuzzer(Buzzer& b) { buzzer = &b; }

    void setup() {
        myservo.setPeriodHertz(50);
        // 物理ピンに接続
        myservo.attach(pin, 500, 2400);

        // 現在の角度(180)を初期値として書き込み
        myservo.write(currentAngle);
        delay(100);

        // 起動時に180度（Front）までゆっくり移動
        moveSlowlyTo(180); 
    }

    // 指定の角度まで音楽を鳴らしながら移動
    void moveSlowlyTo(int targetAngle) {
        targetAngle = constrain(targetAngle, 0, 180);
        
        while (currentAngle != targetAngle) {
            if (currentAngle < targetAngle) currentAngle++;
            else currentAngle--;
            
            myservo.write(currentAngle);

            // 待機中にBGMを更新
            for (int i = 0; i < smoothDelay; i++) {
                if (buzzer != nullptr) {
                    buzzer->playDelfinoPlazaFull();
                }
                delay(1); 
                yield();
            }
        }
    }

    void front()  { moveSlowlyTo(180); }
    void back()   { moveSlowlyTo(0); }
    void center() { moveSlowlyTo(90); }

    void moveFront() {
        if (currentAngle < 180) {
            currentAngle += step;
            if (currentAngle > 180) currentAngle = 180;
            myservo.write(currentAngle);
        }
        if (buzzer != nullptr) buzzer->playDelfinoPlazaFull();
    }

    void moveBack() {
        if (currentAngle > 0) {
            currentAngle -= step;
            if (currentAngle < 0) currentAngle = 0;
            myservo.write(currentAngle);
        }
        if (buzzer != nullptr) buzzer->playDelfinoPlazaFull();
    }

    int getCurrentAngle() { return currentAngle; }
};
#endif