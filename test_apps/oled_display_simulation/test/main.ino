#include <Arduino.h>
#include <Wire.h>
#include <U8g2lib.h>
#include <ESP32Servo.h>
#include "pitches.h"
#include "Buzzer.h"
#include "Pump.h"
#include "Actuator.h"
#include "UI.h"
#include "Sequence.h"

#define JOY_X 32
#define JOY_Y 33
#define JOY_SW 25
#define SERVO_PIN 16
#define BUZZER_PIN 4

U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 22, 21);
Pump pumps(27, 26, 13, 19, 2, 18, 14);
Actuator servo(SERVO_PIN);
UI ui(display, JOY_X, JOY_Y, JOY_SW);
Buzzer buzzer(BUZZER_PIN);
SequenceController seq(pumps, servo, ui, buzzer);

const char* mainM[] = {"1. phase", "2. pump control", "3. actuator control", "4. discharge", "5. close"};
const char* phaseM[] = {"all phase", "washing", "loading", "collecting", "loading & col."};
const char* pumpM[] = {"12ch pump", "pump1", "pump2", "pump3"};

enum MenuPage { MAIN, PHASE, PUMP, ACTUATOR };
MenuPage currentPage = MAIN;
int cursor = 0;
bool pStatus[4] = {false};

void setup() {
    Serial.begin(115200);
    ESP32PWM::allocateTimer(0);
    ESP32PWM::allocateTimer(1);
    display.begin();
    ui.setup();
    buzzer.setup();
    servo.setBuzzer(buzzer); 
    servo.setup();  
    pumps.setup();
    buzzer.play(NOTE_CONFIRM, 150);
}

void loop() {
    Command cmd = ui.getCommand();

    if (currentPage == ACTUATOR) {
        int x = ui.getX();
        if (x > 3500) { servo.moveBack(); delay(15); }
        else if (x < 500) { servo.moveFront(); delay(15); }
        if (ui.isPressed()) { buzzer.play(NOTE_SELECT, 50); currentPage = MAIN; cursor = 2; }

        display.clearBuffer();
        display.setFont(u8g2_font_6x10_tr);
        display.drawStr(0, 10, "ACTUATOR CONTROL");
        char angleMsg[20];
        sprintf(angleMsg, "Angle: %3d deg", servo.getCurrentAngle());
        display.setFont(u8g2_font_ncenB10_tr); 
        display.drawStr(15, 60, angleMsg);
        display.sendBuffer();

    } else {
        int menuSize = (currentPage == PUMP) ? 4 : 5;
        switch (cmd) {
            case UP: cursor = (cursor - 1 + menuSize) % menuSize; buzzer.play(NOTE_SELECT, 30); break;
            case DOWN: cursor = (cursor + 1) % menuSize; buzzer.play(NOTE_SELECT, 30); break;
            case ENTER:
                buzzer.play(NOTE_CONFIRM, 100);
                if (currentPage == MAIN) {
                    if (cursor == 0) currentPage = PHASE;
                    else if (cursor == 1) currentPage = PUMP;
                    else if (cursor == 2) currentPage = ACTUATOR;
                    else if (cursor == 3) {
                        seq.discharge();
                        buzzer.stopMusic(); delay(100); buzzer.play(NOTE_CONFIRM, 200);
                    }
                    else if (cursor == 4) { 
                        buzzer.playCloseSound(); 
                        servo.front(); 
                        display.clearBuffer();
                        display.setFont(u8g2_font_ncenB10_tr);
                        display.drawStr(7, 35, "SYSTEM CLOSED");
                        display.sendBuffer();
                        while(1) { delay(1000); }
                    }
                    cursor = 0;
                } else if (currentPage == PHASE) {
                    if (cursor == 0) seq.allPhase();
                    else if (cursor == 1) seq.washing();
                    else if (cursor == 2) seq.loading();
                    else if (cursor == 3) seq.collecting();
                    else if (cursor == 4) seq.loading_collecting();
                    
                    buzzer.stopMusic(); 
                    delay(100);
                    buzzer.play(NOTE_CONFIRM, 200);
                } else if (currentPage == PUMP) {
                    pStatus[cursor] = !pStatus[cursor];
                    if(cursor == 0) (pStatus[0]) ? pumps.on12ch(false) : pumps.stop12ch();
                    else (pStatus[cursor]) ? pumps.onDBA(cursor) : pumps.offDBA();
                }
                break;
            case BACK:
                buzzer.play(NOTE_SELECT, 50);
                if (currentPage != MAIN) {
                    if (currentPage == PUMP) pumps.stopAll();
                    currentPage = MAIN; cursor = 0;
                }
                break;
        }
        const char* title = (currentPage == MAIN) ? "MAIN" : (currentPage == PHASE) ? "PHASE" : "PUMP";
        const char** items = (currentPage == MAIN) ? mainM : (currentPage == PHASE) ? phaseM : pumpM;
        ui.drawMenu(title, items, menuSize, cursor, (currentPage == PUMP ? pStatus : nullptr));
    }
}