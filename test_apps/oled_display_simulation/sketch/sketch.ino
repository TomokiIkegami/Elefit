#include <Wire.h>
#include <U8g2lib.h>

// ======================================================
// ポート（ピン）割り当て定義
// ======================================================
#define PWM_PORT_M1_1 27   // ポンプA
#define PWM_PORT_M2_1 26   // ポンプB
#define PWM_PORT_M3_1 12   // ポンプC

#define PWM_PORT_M4_1 0    // 6chポンプ右(R) 正転
#define PWM_PORT_M4_2 2    // 6chポンプ右(R) 逆転
#define PWM_PORT_M5_2 15   // 6chポンプ左(L) 正転
#define PWM_PORT_M5_1 14   // 6chポンプ左(L) 逆転

#define STEP_PORT_1   16   // モータ相 A　#要らない
#define STEP_PORT_2   17   // モータ相 A'
#define STEP_PORT_3   18   // モータ相 B
#define STEP_PORT_4   19   // モータ相 B'

#define LIMIT_SWITCH  34   // リミットスイッチ #要らない

#define PIN_X         32   // ジョイスティックX
#define PIN_Y         33   // ジョイスティックY
#define PIN_SW        25   // ジョイスティックSW

#define PIN_LED_FRONT 4    // 前方アクチュエータ
#define PIN_LED_BACK  5    // 後方アクチュエータ #1つでいいらしい　5V　D系に接続

#define DELAYTIME     8    // シミュレーション用に少し高速化　要らない
#define ELEMENTS_NUM  26   //いったん無視

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

// ======================================================
// 🌟短縮バージョン用時間設定 (単位：秒)
// ======================================================
unsigned long t_11=2, t_12=3, t_13=10, t_14=2; // Washing (本来は計700秒弱)
unsigned long t_21=2, t_22=3, t_23=5,  t_24=5, t_25=1, t_26=2; // Loading (本来は計700秒強)
unsigned long t_31=2, t_32=3, t_33=1,  t_34=4, t_35=1, t_36=5; // Collecting
unsigned long t_41=2, t_42=5; // Discharge

// --- 状態管理 ---
enum MenuPage { PAGE_MAIN, PAGE_PHASE, PAGE_PUMP, PAGE_ACTUATOR };
MenuPage currentPage = PAGE_MAIN;
int cursorIndex = 0;
bool pumpStatus[4] = {false, false, false, false};
unsigned long last_btn_time = 0;

const char* mainMenu[] = {"1. phase", "2. pump control", "3. actuator control", "4. discharge", "5. close"};
const char* phaseMenu[] = {"all phase", "washing", "loading", "collecting", "loading & col."};
const char* pumpMenu[] = {"12ch pump", "pump1", "pump2", "pump3"};

void(*resetFunc)(void) = 0;

// ======================================================
// 駆動関数
// ======================================================

void step_front(int step) {
  digitalWrite(STEP_PORT_2, HIGH); digitalWrite(STEP_PORT_4, HIGH);
  for (int i = 0; i < step; i++) {
    digitalWrite(STEP_PORT_1, HIGH); delay(DELAYTIME);
    digitalWrite(STEP_PORT_3, HIGH); delay(DELAYTIME);
    digitalWrite(STEP_PORT_1, LOW);  delay(DELAYTIME);
    digitalWrite(STEP_PORT_3, LOW);  delay(DELAYTIME);
  }
  digitalWrite(STEP_PORT_2, LOW); digitalWrite(STEP_PORT_4, LOW);
}

void step_back(int step) {
  digitalWrite(STEP_PORT_2, HIGH); digitalWrite(STEP_PORT_4, HIGH);
  for (int i = 0; i < step; i++) {
    if (digitalRead(LIMIT_SWITCH) == HIGH) break; 
    digitalWrite(STEP_PORT_3, HIGH); delay(DELAYTIME);
    digitalWrite(STEP_PORT_1, HIGH); delay(DELAYTIME);
    digitalWrite(STEP_PORT_3, LOW);  delay(DELAYTIME);
    digitalWrite(STEP_PORT_1, LOW);  delay(DELAYTIME);
  }
  digitalWrite(STEP_PORT_2, LOW); digitalWrite(STEP_PORT_4, LOW);
}

void on_pump_dba(int id) {
  if(id==1) digitalWrite(PWM_PORT_M1_1, HIGH);
  if(id==2) digitalWrite(PWM_PORT_M2_1, HIGH);
  if(id==3) digitalWrite(PWM_PORT_M3_1, HIGH);
}
void off_pump_dba() { digitalWrite(PWM_PORT_M1_1, LOW); digitalWrite(PWM_PORT_M2_1, LOW); digitalWrite(PWM_PORT_M3_1, LOW); }

void on_pump_12ch(bool rev) {
  if(!rev) { digitalWrite(PWM_PORT_M4_1, HIGH); digitalWrite(PWM_PORT_M5_2, HIGH); }
  else     { digitalWrite(PWM_PORT_M4_2, HIGH); digitalWrite(PWM_PORT_M5_1, HIGH); }
}
void off_pump_12ch() { 
  digitalWrite(PWM_PORT_M4_1, LOW); digitalWrite(PWM_PORT_M4_2, LOW);
  digitalWrite(PWM_PORT_M5_1, LOW); digitalWrite(PWM_PORT_M5_2, LOW);
}

void pwm_pump_12ch(unsigned long run_time_sec) {
  unsigned long start = millis();
  while (millis() - start <= run_time_sec * 1000) {
    on_pump_12ch(false); delay(200); // 脈動もシミュレーション用に高速化
    off_pump_12ch();     delay(300);
  }
}

// ======================================================
// 各工程シーケンス
// ======================================================

void info(const char* m) { 
  u8g2.clearBuffer(); u8g2.drawStr(10,35,m); u8g2.sendBuffer(); 
  Serial.println(m); 
}

void washing() {
  info("Washing...");
  step_front(30); step_back(100);
  on_pump_dba(1); delay(t_11 * 1000); off_pump_dba();
  on_pump_12ch(false); delay(t_12 * 1000); off_pump_12ch();
  pwm_pump_12ch(t_13);
  on_pump_12ch(false); delay(t_14 * 1000); off_pump_12ch();
}

void loading() {
  info("Loading...");
  on_pump_dba(2); delay(t_21 * 1000); off_pump_dba();
  on_pump_12ch(false); delay(t_22 * 1000); off_pump_12ch();
  delay(t_23 * 1000);
  on_pump_12ch(false); delay(t_24 * 1000); off_pump_12ch();
  for(int i=0; i<2; i++) { // 回数も短縮
    on_pump_dba(3); delay(t_25 * 1000); off_pump_dba();
    on_pump_12ch(false); delay(t_26 * 1000); off_pump_12ch();
  }
}

void collecting() {
  info("Collecting...");
  on_pump_dba(3); delay(t_31 * 1000); off_pump_dba();
  on_pump_12ch(false); delay(t_32 * 1000); off_pump_12ch();
  on_pump_12ch(true);  delay(t_33 * 1000); off_pump_12ch();
  step_front(50); on_pump_12ch(false); delay(t_34 * 1000); off_pump_12ch();
  on_pump_12ch(true);  delay(t_35 * 1000); off_pump_12ch();
  step_back(50);  on_pump_12ch(false); delay(t_36 * 1000); off_pump_12ch();
}

void discharge() {
  info("Discharge...");
  step_front(20); step_back(50);
  on_pump_dba(1); on_pump_dba(2); on_pump_dba(3);
  delay(t_41 * 1000); off_pump_dba();
  on_pump_12ch(false); delay(t_42 * 1000); off_pump_12ch();
}

void all_phase() { washing(); loading(); collecting(); info("All Done!"); delay(2000); }

// ======================================================
// メインループ
// ======================================================

void setup() {
  Serial.begin(115200); u8g2.begin(); u8g2.setFont(u8g2_font_6x10_tr);
  pinMode(PWM_PORT_M1_1, OUTPUT); pinMode(PWM_PORT_M2_1, OUTPUT); pinMode(PWM_PORT_M3_1, OUTPUT);
  pinMode(PWM_PORT_M4_1, OUTPUT); pinMode(PWM_PORT_M4_2, OUTPUT);
  pinMode(PWM_PORT_M5_1, OUTPUT); pinMode(PWM_PORT_M5_2, OUTPUT);
  pinMode(STEP_PORT_1, OUTPUT);   pinMode(STEP_PORT_2, OUTPUT);
  pinMode(STEP_PORT_3, OUTPUT);   pinMode(STEP_PORT_4, OUTPUT);
  pinMode(PIN_LED_FRONT, OUTPUT); pinMode(PIN_LED_BACK, OUTPUT);
  pinMode(LIMIT_SWITCH, INPUT_PULLUP); pinMode(PIN_SW, INPUT_PULLUP);
}

void loop() {
  int x = analogRead(PIN_X), y = analogRead(PIN_Y);
  bool sw = !digitalRead(PIN_SW);
  
  if (millis() - last_btn_time > 250) {
    if (currentPage != PAGE_ACTUATOR) {
      int size = (currentPage == PAGE_MAIN) ? 5 : (currentPage == PAGE_PHASE) ? 5 : 4;
      if (y > 3500) { cursorIndex = (cursorIndex - 1 + size) % size; last_btn_time = millis(); }
      if (y < 500)  { cursorIndex = (cursorIndex + 1) % size; last_btn_time = millis(); }
      
      if (sw || x < 500) { 
        last_btn_time = millis();
        if (currentPage == PAGE_MAIN) {
          if (cursorIndex == 0) { currentPage = PAGE_PHASE; cursorIndex = 0; }
          else if (cursorIndex == 1) { currentPage = PAGE_PUMP; cursorIndex = 0; }
          else if (cursorIndex == 2) { currentPage = PAGE_ACTUATOR; cursorIndex = 0; }
          else if (cursorIndex == 3) { discharge(); }
          else if (cursorIndex == 4) { resetFunc(); }
        } 
        else if (currentPage == PAGE_PHASE) {
          if (cursorIndex == 0) all_phase();
          else if (cursorIndex == 1) washing();
          else if (cursorIndex == 2) loading();
          else if (cursorIndex == 3) collecting();
          else if (cursorIndex == 4) { loading(); collecting(); }
        }
        else if (currentPage == PAGE_PUMP) {
          pumpStatus[cursorIndex] = !pumpStatus[cursorIndex];
          if(cursorIndex==0) { if(pumpStatus[0]) on_pump_12ch(false); else off_pump_12ch(); }
          if(cursorIndex==1) digitalWrite(PWM_PORT_M1_1, pumpStatus[1]);
          if(cursorIndex==2) digitalWrite(PWM_PORT_M2_1, pumpStatus[2]);
          if(cursorIndex==3) digitalWrite(PWM_PORT_M3_1, pumpStatus[3]);
        }
      }
      if (x > 3500 && currentPage != PAGE_MAIN) { currentPage = PAGE_MAIN; cursorIndex = 0; last_btn_time = millis(); }
    }
  }

  if (currentPage == PAGE_ACTUATOR) {
    digitalWrite(PIN_LED_FRONT, (x > 3500));
    digitalWrite(PIN_LED_BACK, (x < 500));
    if (sw) { currentPage = PAGE_MAIN; cursorIndex = 2; delay(300); }
  }

  u8g2.clearBuffer();
  if (currentPage == PAGE_ACTUATOR) {
    u8g2.drawStr(0,10,"[ 3. Actuator ]"); u8g2.drawStr(10,40,"L:FRONT R:BACK");
  } else {
    const char* title = (currentPage == PAGE_MAIN) ? "- MAIN MENU -" : (currentPage == PAGE_PHASE) ? "[ 1. Phase ]" : "[ 2. Pump ]";
    u8g2.drawStr(0,10,title);
    const char** d = (currentPage == PAGE_MAIN) ? mainMenu : (currentPage == PAGE_PHASE) ? phaseMenu : pumpMenu;
    int s = (currentPage == PAGE_MAIN) ? 5 : (currentPage == PAGE_PHASE) ? 5 : 4;
    for (int i=0; i<s; i++) {
      if (i == cursorIndex) u8g2.drawStr(0, 24+i*10, ">");
      u8g2.drawStr(10, 24+i*10, d[i]);
      if (currentPage == PAGE_PUMP) u8g2.drawStr(90, 24+i*10, pumpStatus[i] ? "[ON]" : "OFF");
    }
  }
  u8g2.sendBuffer();
}