#include <Wire.h>
#include <U8g2lib.h>

U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0, /* reset=*/ U8X8_PIN_NONE);

const int PIN_X  = 32;
const int PIN_Y  = 33;
const int PIN_SW = 25;

// アクチュエータ用LED
const int PIN_LED_FRONT = 4;
const int PIN_LED_BACK  = 5;

// 🌟追加：ポンプ用LEDのピン設定
const int PIN_LED_12CH  = 14;
const int PIN_LED_PUMP1 = 27;
const int PIN_LED_PUMP2 = 26;
const int PIN_LED_PUMP3 = 12;

enum ButtonState {
  BTN_NONE, BTN_UP, BTN_DOWN, BTN_LEFT, BTN_RIGHT, BTN_ENTER
};

enum MenuPage {
  PAGE_MAIN, PAGE_PHASE, PAGE_PUMP, PAGE_ACTUATOR
};

MenuPage currentPage = PAGE_MAIN;
int cursorIndex = 0;

// 🌟追加：4つのポンプが現在ONかOFFかを記憶する変数（最初はすべてOFF＝false）
bool pumpStatus[4] = {false, false, false, false};

// 🌟変更：「operation」を「control」に変更
const int MAIN_MENU_SIZE = 5;
const char* mainMenu[] = {"1. phase", "2. pump control", "3. actuator control", "4. discharge", "5. close"};

const int PHASE_MENU_SIZE = 5;
const char* phaseMenu[] = {"all phase", "washing", "loading", "collecting", "loading & collecting"};

const int PUMP_MENU_SIZE = 4;
const char* pumpMenu[] = {"12ch pump", "pump1", "pump2", "pump3"};

void setup() {
  Serial.begin(115200);
  u8g2.begin();
  analogReadResolution(10); 
  pinMode(PIN_X, INPUT);
  pinMode(PIN_Y, INPUT);
  pinMode(PIN_SW, INPUT_PULLUP);
  
  pinMode(PIN_LED_FRONT, OUTPUT);
  pinMode(PIN_LED_BACK, OUTPUT);
  
  // 🌟追加：ポンプ用LEDを出力モードに設定
  pinMode(PIN_LED_12CH, OUTPUT);
  pinMode(PIN_LED_PUMP1, OUTPUT);
  pinMode(PIN_LED_PUMP2, OUTPUT);
  pinMode(PIN_LED_PUMP3, OUTPUT);
}

ButtonState getButton() {
  int x = analogRead(PIN_X);
  int y = analogRead(PIN_Y);
  bool sw = !digitalRead(PIN_SW); 

  if (sw) return BTN_ENTER;
  if (x > 800) return BTN_LEFT;
  if (x < 200) return BTN_RIGHT;
  if (y > 800) return BTN_UP;
  if (y < 200) return BTN_DOWN;
  return BTN_NONE;
}

ButtonState lastBtn = BTN_NONE;

void loop() {
  ButtonState btn = getButton();
  
  if (currentPage == PAGE_ACTUATOR) {
    // --- アクチュエータ画面の処理（変更なし） ---
    if (btn == BTN_LEFT) {
      digitalWrite(PIN_LED_FRONT, HIGH);
      digitalWrite(PIN_LED_BACK, LOW);
    } else if (btn == BTN_RIGHT) {
      digitalWrite(PIN_LED_FRONT, LOW);
      digitalWrite(PIN_LED_BACK, HIGH); 
    } else {
      digitalWrite(PIN_LED_FRONT, LOW);
      digitalWrite(PIN_LED_BACK, LOW);
    }

    if (btn != lastBtn && (btn == BTN_ENTER || btn == BTN_DOWN)) {
      digitalWrite(PIN_LED_FRONT, LOW);
      digitalWrite(PIN_LED_BACK, LOW);
      currentPage = PAGE_MAIN;
      cursorIndex = 2; // 「3. actuator control」に戻る
    }

  } else {
    // --- 通常のリストメニューの処理 ---
    if (btn != BTN_NONE && btn != lastBtn) {
      
      int currentMenuSize = 0;
      if (currentPage == PAGE_MAIN) currentMenuSize = MAIN_MENU_SIZE;
      else if (currentPage == PAGE_PHASE) currentMenuSize = PHASE_MENU_SIZE;
      else if (currentPage == PAGE_PUMP) currentMenuSize = PUMP_MENU_SIZE;

      if (btn == BTN_DOWN) {
        cursorIndex++;
        if (cursorIndex >= currentMenuSize) cursorIndex = 0;
      }
      if (btn == BTN_UP) {
        cursorIndex--;
        if (cursorIndex < 0) cursorIndex = currentMenuSize - 1;
      }

      // 決定（右 または 押し込み）を入力した時の処理
      if (btn == BTN_ENTER || btn == BTN_RIGHT) {
        if (currentPage == PAGE_MAIN) {
          // メインメニューからの画面遷移
          if (cursorIndex == 0) { currentPage = PAGE_PHASE; cursorIndex = 0; }
          else if (cursorIndex == 1) { currentPage = PAGE_PUMP; cursorIndex = 0; }
          else if (cursorIndex == 2) { currentPage = PAGE_ACTUATOR; cursorIndex = 0; }
          
        } else if (currentPage == PAGE_PUMP) {
          // 🌟追加：ポンプ画面で決定を押すと、そのポンプのON/OFFが反転する
          pumpStatus[cursorIndex] = !pumpStatus[cursorIndex];
          
          // 変数の状態に合わせて実際のLEDを光らせる
          digitalWrite(PIN_LED_12CH,  pumpStatus[0] ? HIGH : LOW);
          digitalWrite(PIN_LED_PUMP1, pumpStatus[1] ? HIGH : LOW);
          digitalWrite(PIN_LED_PUMP2, pumpStatus[2] ? HIGH : LOW);
          digitalWrite(PIN_LED_PUMP3, pumpStatus[3] ? HIGH : LOW);
        }
      }

      // 戻る（左）を入力した時の処理
      if (btn == BTN_LEFT) {
        if (currentPage != PAGE_MAIN) {
          int prevPage = currentPage;
          currentPage = PAGE_MAIN;
          // 戻ったときに、元いたメニューの位置にカーソルを合わせてあげる親切設計
          if (prevPage == PAGE_PHASE) cursorIndex = 0;
          if (prevPage == PAGE_PUMP) cursorIndex = 1;
        }
      }
    }
  }
  
  lastBtn = btn; 

  // --- OLEDへの描画処理 ---
  u8g2.clearBuffer();
  
  if (currentPage == PAGE_ACTUATOR) {
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(0, 10, "[ 3. actuator ]");
    u8g2.drawLine(0, 12, 128, 12);
    
    u8g2.setFont(u8g2_font_8x13_tr); 
    if (btn == BTN_LEFT)  u8g2.drawStr(10, 35, "[FRONT]"); else u8g2.drawStr(10, 35, "<-FRONT");
    if (btn == BTN_RIGHT) u8g2.drawStr(70, 35, "[BACK]");  else u8g2.drawStr(70, 35, "BACK->");
    
    u8g2.setFont(u8g2_font_6x10_tr);
    u8g2.drawStr(35, 60, "(O) MENU"); 

  } else {
    u8g2.setFont(u8g2_font_6x10_tr);
    const char** currentMenuData;
    int menuSize;
    const char* title;

    if (currentPage == PAGE_MAIN) {
      currentMenuData = mainMenu; menuSize = MAIN_MENU_SIZE; title = "- MAIN MENU -";
    } else if (currentPage == PAGE_PHASE) {
      currentMenuData = phaseMenu; menuSize = PHASE_MENU_SIZE; title = "[ 1. phase ]";
    } else if (currentPage == PAGE_PUMP) {
      currentMenuData = pumpMenu; menuSize = PUMP_MENU_SIZE; title = "[ 2. pump ]";
    }

    u8g2.drawStr(0, 10, title);
    u8g2.drawLine(0, 12, 128, 12); 

    int startY = 24; 
    int lineHeight = 10; 

    for (int i = 0; i < menuSize; i++) {
      int y = startY + (i * lineHeight);
      if (i == cursorIndex) u8g2.drawStr(0, y, ">");
      u8g2.drawStr(10, y, currentMenuData[i]);
      
      // 🌟追加：ポンプ画面の時だけ、各項目の右側に現在のON/OFF状態を表示する
      if (currentPage == PAGE_PUMP) {
        if (pumpStatus[i] == true) {
          u8g2.drawStr(90, y, "[ON]");
        } else {
          u8g2.drawStr(90, y, " OFF ");
        }
      }
    }
  }

  u8g2.sendBuffer();
}