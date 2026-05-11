#ifndef SEQUENCE_H
#define SEQUENCE_H

#include "Pump.h"
#include "Actuator.h"
#include "UI.h"
#include "Buzzer.h"
#include "pitches.h"

#define SIM_SPEED_SCALE 20 

#define WAIT(sec) { \
    unsigned long startWait = millis(); \
    unsigned long targetWait = (unsigned long)((sec * 1000) / SIM_SPEED_SCALE); \
    while (millis() - startWait < targetWait) { \
        buzzer.playDelfinoPlazaFull(); \
        delay(1); \
    } \
}

class SequenceController {
private:
    Pump& pumps;
    Actuator& servo;
    UI& ui;
    Buzzer& buzzer;

    const unsigned long t_11 = 18, t_12 = 45, t_13 = 600, t_14 = 30;
    const unsigned long t_21 = 19, t_22 = 45, t_23 = 300, t_24 = 250, t_25 = 4, t_26 = 60;
    const unsigned long t_31 = 15, t_32 = 45, t_33 = 5,   t_34 = 60,  t_35 = 5, t_36 = 100;
    const unsigned long t_41 = 7,  t_42 = 280;

    void updateStatus(const char* displayMsg, const char* debugLog) {
        ui.drawStatus(displayMsg);
        Serial.print("[DEBUG] "); 
        Serial.println(debugLog);
    }

    void pwm_pump_12ch(unsigned long duration_sec, const char* displayMsg, const char* debugLog) {
        updateStatus(displayMsg, debugLog);
        unsigned long start = millis();
        unsigned long total_ms = (duration_sec * 1000) / SIM_SPEED_SCALE;
        while (millis() - start <= total_ms) {
            pumps.on12ch(true); 
            unsigned long t = millis();
            while(millis() - t < (385 / SIM_SPEED_SCALE)) { 
                buzzer.playDelfinoPlazaFull(); 
                delay(1); 
            }
            pumps.stop12ch();    
            t = millis();
            while(millis() - t < (625 / SIM_SPEED_SCALE)) { 
                buzzer.playDelfinoPlazaFull(); 
                delay(1); 
            }
        }
    }

public:
    SequenceController(Pump& p, Actuator& s, UI& u, Buzzer& b) 
        : pumps(p), servo(s), ui(u), buzzer(b) {}

    void washing() {
        buzzer.startWashing(); 
        servo.front();
        updateStatus("Washing...", "1-1"); pumps.onDBA(1); WAIT(t_11); pumps.offDBA();
        updateStatus("Washing...", "1-2"); pumps.on12ch(true); WAIT(t_12); pumps.stop12ch();
        pwm_pump_12ch(t_13, "Washing...", "1-3");
        updateStatus("Washing...", "1-4"); pumps.on12ch(true); WAIT(t_14); pumps.stop12ch();
    }

    void loading() {
        buzzer.startLoading(); 
        servo.front();
        updateStatus("Loading...", "2-1"); pumps.onDBA(2); WAIT(t_21); pumps.offDBA();
        updateStatus("Loading...", "2-2"); pumps.on12ch(true); WAIT(t_22); pumps.stop12ch();
        updateStatus("Loading...", "2-3"); WAIT(t_23); 
        updateStatus("Loading...", "2-4"); pumps.on12ch(true); WAIT(t_24); pumps.stop12ch();
        for(int i=0; i<3; i++) {
            char debugBuf[16];
            sprintf(debugBuf, "2-5-%d", i+1);
            updateStatus("Loading (DBA)...", debugBuf); 
            pumps.onDBA(3); WAIT(t_25); pumps.offDBA();
            sprintf(debugBuf, "2-6-%d", i+1);
            updateStatus("Loading (12ch)...", debugBuf); 
            pumps.on12ch(true); WAIT(t_26); pumps.stop12ch();
        }
    }

    void collecting() {
        buzzer.startCollecting(); 
        updateStatus("Collecting...", "3-1"); pumps.onDBA(3); WAIT(t_31); pumps.offDBA();
        updateStatus("Collecting...", "3-2"); pumps.on12ch(true); WAIT(t_32); pumps.stop12ch();
        updateStatus("Collecting...", "3-3"); pumps.on12ch(false); WAIT(t_33); pumps.stop12ch();
        updateStatus("Collecting (M)...", "3-4"); servo.back(); pumps.on12ch(true); WAIT(t_34); pumps.stop12ch();
        updateStatus("Collecting (M)...", "3-5"); servo.front(); pumps.on12ch(false); WAIT(t_35); pumps.stop12ch();
        updateStatus("Collecting...", "3-6"); pumps.on12ch(true); WAIT(t_36); pumps.stop12ch();
    }

    void discharge() {
        buzzer.startDischarge(); 
        servo.front();
        updateStatus("Discharge...", "4-1"); pumps.onDBA(1); pumps.onDBA(2); pumps.onDBA(3);
        WAIT(t_41); pumps.offDBA();
        updateStatus("Discharge...", "4-2"); pumps.on12ch(true); WAIT(t_42); pumps.stop12ch();
    }

    void allPhase() {
        washing(); 
        loading(); 
        collecting(); 
        ui.drawStatus("ALL DONE"); 
    }

    void loading_collecting() {
        loading(); 
        collecting(); 
        ui.drawStatus("L+C DONE"); 
    }
};
#endif