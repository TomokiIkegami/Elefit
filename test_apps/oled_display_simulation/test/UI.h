#ifndef UI_H
#define UI_H
#include <U8g2lib.h>
#include <Arduino.h>

// 追記：ボタン入力の結果をメインに伝えるための型
enum Command { NONE, UP, DOWN, ENTER, BACK };

class UI {
private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C& u8g2;
    uint8_t pinX, pinY, pinSW;
    unsigned long lastAction = 0;
    const int thresholdLow = 500;
    const int thresholdHigh = 3500;

public:
    UI(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& display, uint8_t x, uint8_t y, uint8_t sw) 
        : u8g2(display), pinX(x), pinY(y), pinSW(sw) {}

    void setup() {
        u8g2.begin();
        u8g2.setFont(u8g2_font_6x10_tr);
        pinMode(pinSW, INPUT_PULLUP);
    }

    int getX() { return analogRead(pinX); }
    int getY() { return analogRead(pinY); }
    bool isPressed() { return !digitalRead(pinSW); }

    // 追記：入力状態を判定して「コマンド」として返す
    Command getCommand() {
        if (millis() - lastAction < 250) return NONE; // チャタリング・連続入力防止

        int x = getX();
        int y = getY();
        bool sw = isPressed();

        if (sw || x < thresholdLow) { // 右倒し or 押し込み
            lastAction = millis();
            return ENTER;
        }
        if (x > thresholdHigh) { // 左倒し
            lastAction = millis();
            return BACK;
        }
        if (y > thresholdHigh) { // 上倒し
            lastAction = millis();
            return UP;
        }
        if (y < thresholdLow) { // 下倒し
            lastAction = millis();
            return DOWN;
        }

        return NONE;
    }

    void drawStatus(const char* m) {
        u8g2.clearBuffer();
        u8g2.drawStr(10, 35, m);
        u8g2.sendBuffer();
    }

    void drawMenu(const char* title, const char** items, int size, int cursor, bool* pumpStatus = nullptr) {
        u8g2.clearBuffer();
        u8g2.drawStr(0, 10, title);
        for (int i = 0; i < size; i++) {
            int yPos = 24 + (i * 10);
            if (i == cursor) u8g2.drawStr(0, yPos, ">");
            u8g2.drawStr(10, yPos, items[i]);
            if (pumpStatus != nullptr) {
                u8g2.drawStr(90, yPos, pumpStatus[i] ? "[ON]" : "OFF");
            }
        }
        u8g2.sendBuffer();
    }
};
#endif