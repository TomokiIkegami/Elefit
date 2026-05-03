#ifndef PUMP_H
#define PUMP_H
#include <Arduino.h>

class Pump {
private:
    uint8_t pA, pB, pC, rF, rR, lF, lR;
public:
    Pump(uint8_t a, uint8_t b, uint8_t c, uint8_t rf, uint8_t rr, uint8_t lf, uint8_t lr) 
        : pA(a), pB(b), pC(c), rF(rf), rR(rr), lF(lf), lR(lr) {}

    void setup() {
        pinMode(pA, OUTPUT); pinMode(pB, OUTPUT); pinMode(pC, OUTPUT);
        pinMode(rF, OUTPUT); pinMode(rR, OUTPUT); pinMode(lF, OUTPUT); pinMode(lR, OUTPUT);
        stopAll();
    }

    void onDBA(int id) {
        if(id == 1) digitalWrite(pA, HIGH);
        else if(id == 2) digitalWrite(pB, HIGH);
        else if(id == 3) digitalWrite(pC, HIGH);
    }
    void offDBA() { digitalWrite(pA, LOW); digitalWrite(pB, LOW); digitalWrite(pC, LOW); }
    void on12ch(bool rev) {
        if(rev) {digitalWrite(rR, LOW); digitalWrite(lR, LOW); digitalWrite(rF, HIGH); digitalWrite(lF, HIGH);}
        else { digitalWrite(rF, HIGH); digitalWrite(lF, HIGH); digitalWrite(rR, LOW); digitalWrite(lR, LOW); }
    }
    void stop12ch() { digitalWrite(rF, LOW); digitalWrite(rR, LOW); digitalWrite(lF, LOW); digitalWrite(lR, LOW); }
    void stopAll() { offDBA(); stop12ch(); }
};
#endif