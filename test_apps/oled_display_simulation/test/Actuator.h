#ifndef ACTUATOR_H
#define ACTUATOR_H

#include <ESP32Servo.h>
#include <Arduino.h>

class Actuator {
private:
    Servo myservo;
    uint8_t pin;
    int currentAngle;
    const int step = 2; 
    const int smoothDelay = 15; // ゆっくり動かす時の待ち時間（大きいほど遅くなる）

public:
    Actuator(uint8_t p) : pin(p), currentAngle(180) {}

    void setup() {
        ESP32PWM::allocateTimer(0);
        ESP32PWM::allocateTimer(1);
        ESP32PWM::allocateTimer(2);
        ESP32PWM::allocateTimer(3);
        myservo.setPeriodHertz(50);
        myservo.attach(pin, 500, 2400); 
        myservo.write(currentAngle); 
    }

    // ゆっくり指定の角度まで動かす内部関数
    void moveSlowlyTo(int targetAngle) {
        targetAngle = constrain(targetAngle, 0, 180); // 0-180度に制限
        
        while (currentAngle != targetAngle) {
            if (currentAngle < targetAngle) {
                currentAngle++;
            } else {
                currentAngle--;
            }
            myservo.write(currentAngle);
            delay(smoothDelay); // ここでスピードを調整
        }
    }

    // 各動作をゆっくり実行するように変更
    void front()  { moveSlowlyTo(180); }
    void back()   { moveSlowlyTo(0);   }
    void center() { moveSlowlyTo(90);  }

    // スティック右：0度から180度までキッチリ動かす
    void moveFront() {
        if (currentAngle < 180) {
            currentAngle += step;
            if (currentAngle > 180) currentAngle = 180; // 180を超えたら180に固定
            myservo.write(currentAngle);
        }
    }

    // スティック左：0度までキッチリ戻す
    void moveBack() {
        if (currentAngle > 0) {
            currentAngle -= step;
            if (currentAngle < 0) currentAngle = 0;   // 0を下回ったら0に固定
            myservo.write(currentAngle);
        }
    }

    int getCurrentAngle() { return currentAngle; }
};
#endif