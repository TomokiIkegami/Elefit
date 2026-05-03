#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP32Servo.h>
#include "Pump.h"
#include "Actuator.h"
#include "UI.h"
#include "Sequence.h"

// --- ピン定義 ---
#define PUMP_A 27
#define PUMP_B 26
#define PUMP_C 13
#define CH12_RF 19
#define CH12_RR 2
#define CH12_LF 18
#define CH12_LR 14
#define JOY_X 32
#define JOY_Y 33
#define JOY_SW 25
#define SERVO_PIN 16

// --- インスタンス化 ---
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 22, 21);
Pump pumps(PUMP_A, PUMP_B, PUMP_C, CH12_RF, CH12_RR, CH12_LF, CH12_LR);
Actuator servo(SERVO_PIN);
UI ui(display, JOY_X, JOY_Y, JOY_SW);
SequenceController seq(pumps, servo, ui);

// --- メニュー表示用データ ---
const char* mainM[] = {"1. phase", "2. pump control", "3. actuator control", "4. discharge", "5. close"};
const char* phaseM[] = {"all phase", "washing", "loading", "collecting", "loading & col."};
const char* pumpM[] = {"12ch pump", "pump1", "pump2", "pump3"};

// --- 状態管理変数 ---
enum MenuPage { MAIN, PHASE, PUMP, ACTUATOR };
MenuPage currentPage = MAIN;
int cursor = 0;
bool pStatus[4] = {false};
unsigned long lastBtn = 0;

void setup() {
    Serial.begin(115200);
    Wire.begin(21, 22);
    
    // Actuatorクラス内の setup() を呼び出し、内部で myservo.attach(18, 500, 2400) を実行
    servo.setup(); 
    
    pumps.setup();
    ui.setup();
    display.begin();

    Serial.println("System Ready");
}

void loop() {
    Command cmd = ui.getCommand(); // UIクラスからコマンドを取得

    if (currentPage == ACTUATOR) {
        int x = ui.getX();
        // 手動操作はコマンドではなく生のX値で判定（押し続けている間動かすため）
        if (x > 3500) { servo.moveBack(); delay(15); }
        else if (x < 500) { servo.moveFront(); delay(15); }

        if (ui.isPressed()) { // スイッチ押し込みで戻る
            currentPage = MAIN;
            cursor = 2;
        }
        
        // 描画処理（角度表示）
        display.clearBuffer();
        display.setFont(u8g2_font_ncenB08_tr);
        display.drawStr(10, 15, "--- ACTUATOR ---");
        char angleMsg[20];
        sprintf(angleMsg, "Angle: %d deg", servo.getCurrentAngle());
        display.drawStr(10, 55, angleMsg);
        display.sendBuffer();

    } else {
        // --- メニュー操作セクション ---
        int menuSize = (currentPage == PUMP) ? 4 : 5;

        switch (cmd) {
            case UP:
                cursor = (cursor - 1 + menuSize) % menuSize;
                break;
            
            case DOWN:
                cursor = (cursor + 1) % menuSize;
                break;

            case ENTER:
                if (currentPage == MAIN) {
                    if (cursor == 0)      currentPage = PHASE;
                    else if (cursor == 1) currentPage = PUMP;
                    else if (cursor == 2) currentPage = ACTUATOR;
                    else if (cursor == 3) seq.discharge();
                    else if (cursor == 4) ESP.restart();
                    cursor = 0;
                } 
                else if (currentPage == PHASE) {
                    if (cursor == 0) seq.allPhase();
                    // ...他のフェーズ呼び出し
                } 
                else if (currentPage == PUMP) {
                    pStatus[cursor] = !pStatus[cursor];
                    if(cursor == 0) {
                        if(pStatus[0]) pumps.on12ch(false); else pumps.stop12ch();
                    } else {
                        if(pStatus[cursor]) pumps.onDBA(cursor); else pumps.offDBA();
                    }
                }
                break;

            case BACK:
                if (currentPage != MAIN) {
                    // ★ご要望：PUMPメニューから戻る際、全ての動きをOFFにする
                    if (currentPage == PUMP) {
                        pumps.stop12ch();
                        pumps.offDBA();
                        for(int i=0; i<4; i++) pStatus[i] = false;
                        Serial.println("[PUMP] Emergency Stop and Return");
                    }
                    currentPage = MAIN;
                    cursor = 0;
                }
                break;
                
            case NONE:
                break;
        }

        // 描画
        const char* title = (currentPage == MAIN) ? "MAIN" : (currentPage == PHASE) ? "PHASE" : "PUMP";
        const char** items = (currentPage == MAIN) ? mainM : (currentPage == PHASE) ? phaseM : pumpM;
        ui.drawMenu(title, items, menuSize, cursor, (currentPage == PUMP ? pStatus : nullptr));
    }
}