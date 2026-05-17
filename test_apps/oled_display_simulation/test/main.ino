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
#include "Communication.h"

// ジョイスティックおよび各種ペリフェラルのピン番号定義
#define JOY_X 32
#define JOY_Y 33
#define JOY_SW 25
#define SERVO_PIN 16
#define BUZZER_PIN 4

// 各種ハードウェア制御クラスのインスタンス化
U8G2_SSD1306_128X64_NONAME_F_HW_I2C display(U8G2_R0, U8X8_PIN_NONE, 22, 21); // OLEDディスプレイ (I2C)
Pump pumps(27, 26, 13, 19, 2, 18, 14);                                      // 各種ポンプ制御用
Actuator servo(SERVO_PIN);                                                   // サーボモーター制御用
UI ui(display, JOY_X, JOY_Y, JOY_SW);                                        // ジョイスティック・UI描画用
Buzzer buzzer(BUZZER_PIN);                                                   // ブザー・BGM再生用
Communication comm;                                                          // Processing(PC)とのシリアル通信用

// シーケンスコントローラーに各インスタンスを渡し、協調制御できるように紐付ける
SequenceController mainSeq(pumps, servo, ui, buzzer, comm); 

// メニュー画面の文字列配列定義 (Pumpメニューの2番目に12ch Revを持ってきて番号を綺麗に整列)
const char* mainM[]  = {"1.Phase", "2.Pump", "3.Servo", "4.Discharge", "5.Close"};
const char* phaseM[] = {"1.All phase", "2.Wash", "3.Load", "4.Collect", "5.L&C", "6.Main menu"};
const char* pumpM[]  = {"1.12ch fwd", "2.12ch rev", "3.P1", "4.P2", "5.P3", "6.Main menu"};

// 画面遷移管理用の状態定義
enum MenuPage { MAIN, PHASE, PUMP, ACTUATOR };
MenuPage currentPage = MAIN; // 起動時はメインメニューから開始
int cursor = 0;              // メニューの選択カーソル位置

// ポンプStatus保持配列 (全5ポンプ分に整列)
// pStatus[0]: 12ch Fwd, pStatus[1]: 12ch Rev, pStatus[2]: Pump1, pStatus[3]: Pump2, pStatus[4]: Pump3
bool pStatus[5] = {false, false, false, false, false}; 

unsigned long lastStatusTime = 0; // Processingへの進捗定期送信タイマー用
int lastRxCount = 0;         // シリアル割り込みによる意図しない連続リセット防止用

// ボタン状態管理変数
unsigned long buttonPressStartTime = 0; // ボタンが押され始めた時刻を記録する変数
bool isButtonPressing = false;          // 現在ボタンが物理的に押し込まれているかどうかのフラグ
bool triggerEnterAction = false;        // 「ボタンが短く離されたのでENTER処理を実行してよし」という確定トリガー

// 関数ポインタを利用してメモリの0番地へジャンプさせるソフトウェアリセット関数
void(* resetFunc) (void) = 0;

void setup() {
    comm.setup();                 // シリアル通信の初期化 (115200bps)
    ESP32PWM::allocateTimer(0);   // サーボモーター用のPWMタイマー割り当て
    ESP32PWM::allocateTimer(1);
    ui.setup();                   // OLEDディスプレイの開始
    buzzer.setup();               // ブザーピンの出力設定
    ui.drawSystemMessage("Booting...");
    servo.setBuzzer(buzzer);      // サーボ動作音用Buzzerの登録
    servo.setup();                // サーボを初期位置へ移動
    pumps.setup();                // ポンプ制御ピンの初期化・すべて停止
    ui.drawSystemMessage("Ready", true);
    buzzer.play(NOTE_CONFIRM, 150); // 起動完了通知音
    delay(800);                   // 起動画面の視認用ウェイト
}

void loop() {
    // 1. PC (Processing) からの通信コマンド受信処理
    bool hasSerial = comm.update();
    if (hasSerial) {
        if (comm.getRxCount() >= lastRxCount + 2) resetFunc();
        lastRxCount = comm.getRxCount();
        String sCmd = comm.getCommand();

        if (sCmd == "washing")         { mainSeq.washing(); }
        else if (sCmd == "loading")    { mainSeq.loading(); }
        else if (sCmd == "collecting") { mainSeq.collecting(); }
        else if (sCmd == "discharge")  { mainSeq.discharge(); }
        else if (sCmd == "all_phase")  { mainSeq.allPhase(); }
        else if (sCmd == "lc")         { mainSeq.loading_collecting(); }
        
        // PC画面の「pump_12ch」トグルおよび「reverse」切り替えスライドスイッチとシリアル連動
        else if (sCmd == "on_pump_12ch") { 
            bool isRev = (comm.getParam2() == 1); 
            pumps.stop12ch();                     // 一度安全に完全停止
            pumps.on12ch(100, isRev);             // 指定された方向（FwdまたはRev）で常時駆動開始
            
            // 本体ディスプレイ側のメニュー表示のトグル状態(On/Off表記)も、PCの操作とリアルタイムに同期
            if (!isRev) { pStatus[0] = true;  pStatus[1] = false; } 
            else        { pStatus[0] = false; pStatus[1] = true;  }
        }
        else if (sCmd == "off_pump_12ch") { 
            pumps.stop12ch(); 
            pStatus[0] = false; 
            pStatus[1] = false; 
        }
        
        else if (sCmd == "step_front") { servo.moveFront(); }
        else if (sCmd == "step_back")  { servo.moveBack(); }
        else if (sCmd == "on_pump_dba") { pumps.onDBA(comm.getParam1()); }
        else if (sCmd == "off_pump_dba"){ pumps.offDBA(); }
        else if (sCmd == "reset_arduino" || sCmd == "close") { resetFunc(); } 
        
        buzzer.stopMusic(); delay(100); buzzer.play(NOTE_CONFIRM, 200);
    }

    // 2. 待機中の長押しリセット ＆ 離したときだけ決定（先読み・誤爆防止）判定
    if (currentPage != ACTUATOR) { 
        if (!digitalRead(JOY_SW)) { 
            if (!isButtonPressing) {
                buttonPressStartTime = millis(); 
                isButtonPressing = true;
            } else if (millis() - buttonPressStartTime >= 2000) { 
                ui.drawSystemMessage("RESET", true);
                buzzer.play(NOTE_ERROR, 500); 
                delay(500);
                resetFunc(); 
            }
        } else {
            if (isButtonPressing) {
                unsigned long pressDuration = millis() - buttonPressStartTime;
                if (pressDuration > 10 && pressDuration < 2000) {
                    triggerEnterAction = true; 
                }
                isButtonPressing = false; 
            }
        }
    }

    // 3. 画面モードごとの操作およびUI描画処理
    if (currentPage == ACTUATOR) {
        ui.drawServoMenu(servo.getCurrentAngle(), cursor);
        if (cursor == 0 && !digitalRead(JOY_SW)) { servo.moveFront(); delay(10); }
        else if (cursor == 1 && !digitalRead(JOY_SW)) { servo.moveBack(); delay(10); }

        Command sCmd = ui.getCommand(); 
        if (sCmd == LEFT) { cursor = 0; buzzer.play(NOTE_SELECT, 30); }
        else if (sCmd == RIGHT) { cursor = 1; buzzer.play(NOTE_SELECT, 30); }
        else if (sCmd == DOWN) { cursor = 2; buzzer.play(NOTE_SELECT, 30); }
        else if (sCmd == UP && cursor == 2) { cursor = 0; buzzer.play(NOTE_SELECT, 30); }
        else if (sCmd == ENTER && cursor == 2) { 
            buzzer.play(NOTE_CONFIRM, 100); currentPage = MAIN; cursor = 2; 
        }
    } else {
        // メニューサイズ定義 (MAINは5、PHASEは6、PUMPは6項目)
        int menuSize = (currentPage == MAIN) ? 5 : (currentPage == PHASE) ? 6 : 6;
        Command cmd = ui.getCommand();

        if (cmd == ENTER) {
            cmd = NONE; 
        }

        bool isEnterTriggered = triggerEnterAction;
        triggerEnterAction = false; 

        switch (cmd) {
            case UP:   if (!isButtonPressing) { cursor = (cursor - 1 + menuSize) % menuSize; buzzer.play(NOTE_SELECT, 30); } break;
            case DOWN: if (!isButtonPressing) { cursor = (cursor + 1) % menuSize; buzzer.play(NOTE_SELECT, 30); } break;
            case LEFT: if (!isButtonPressing && cursor >= 3) { cursor -= 3; buzzer.play(NOTE_SELECT, 30); } break;
            case RIGHT: if (!isButtonPressing) {
                            if (cursor + 3 < menuSize) { cursor += 3; buzzer.play(NOTE_SELECT, 30); }
                            else if ((cursor / 3) < ((menuSize-1) / 3)) { cursor = menuSize-1; buzzer.play(NOTE_SELECT, 30); }
                            buzzer.play(NOTE_SELECT, 30); 
                        }
                        break;
        }

        if (isEnterTriggered) {
            buzzer.play(NOTE_CONFIRM, 100);
            
            if (currentPage == MAIN) {
                if (cursor == 0)      currentPage = PHASE;
                else if (cursor == 1) currentPage = PUMP;
                else if (cursor == 2) { currentPage = ACTUATOR; cursor = 0; }
                else if (cursor == 3) { mainSeq.discharge(); buzzer.stopMusic(); delay(100); buzzer.play(NOTE_CONFIRM, 200); }
                else if (cursor == 4) { 
                    buzzer.play(NOTE_ERROR, 500); 
                    servo.front(); 
                    ui.drawSystemMessage("RESET", true);
                    delay(2000); 
                    ui.clearScreen(); 
                    resetFunc(); 
                }
                if (currentPage != ACTUATOR) cursor = 0;
            } 
            else if (currentPage == PHASE) {
                if (cursor == 5) { currentPage = MAIN; cursor = 0; }
                else {
                    if (cursor == 0) mainSeq.allPhase(); else if (cursor == 1) mainSeq.washing();
                    else if (cursor == 2) mainSeq.loading(); else if (cursor == 3) mainSeq.collecting();
                    else if (cursor == 4) mainSeq.loading_collecting();
                    buzzer.stopMusic(); delay(100); buzzer.play(NOTE_CONFIRM, 200);
                }
            } 
            else if (currentPage == PUMP) {
                // 「6.Main menu」を選択時：すべてを止めて戻る
                if (cursor == 5) { 
                    pumps.stopAll(); 
                    for(int i=0; i<5; i++) pStatus[i] = false;
                    currentPage = MAIN; 
                    cursor = 1; 
                }
                else {
                    // 各項目のON/OFFトグル切り替え
                    pStatus[cursor] = !pStatus[cursor]; 
                    
                    if (cursor == 0) {
                        // 1.12ch Fwd (正回転)
                        if (pStatus[0]) {
                            pStatus[1] = false; // 2番目の逆転フラグを強制OFF
                            pumps.stop12ch();   // 安全停止
                            pumps.on12ch(100, false); 
                        } else {
                            pumps.stop12ch();
                        }
                    } 
                    else if (cursor == 1) {
                        // 2.12ch Rev (逆回転)
                        if (pStatus[1]) {
                            pStatus[0] = false; // 1番目の正転フラグを強制OFF
                            pumps.stop12ch();   // 安全停止
                            pumps.on12ch(100, true); 
                        } else {
                            pumps.stop12ch();
                        }
                    } 
                    else {
                        // 3~5: DBAポンプ群 (インデックス2=Pump1, 3=Pump2, 4=Pump3)
                        // メニュー上のインデックスから2を引いた数値をIDとして渡す
                        int pumpId = cursor - 1; 
                        if (pStatus[cursor]) pumps.onDBA(pumpId); 
                        else                 pumps.offDBA(); // 引数なしで全DBA一括消灯するためこれで適合
                    }
                }
            }
        }
        
        // メニュー画面全体のバッファ描画更新
        ui.drawMenu((currentPage == MAIN) ? "Main" : (currentPage == PHASE) ? "Phase" : "Pump", 
                    (currentPage == MAIN) ? mainM : (currentPage == PHASE) ? phaseM : pumpM, 
                    menuSize, cursor, (currentPage == PUMP ? pStatus : nullptr));
    }

    // 4. 通常待機時（IDLE）のProcessing向けステータス定期送信 (500msおき)
    if (millis() - lastStatusTime >= 500) {
        lastStatusTime = millis();
        int w=0, l=0, c=0, d=0;
        if (strcmp(mainSeq.activePhase, "washing") == 0) w = mainSeq.currentPercent;
        else if (strcmp(mainSeq.activePhase, "loading") == 0) l = mainSeq.currentPercent;
        else if (strcmp(mainSeq.activePhase, "collecting") == 0) c = mainSeq.currentPercent;
        else if (strcmp(mainSeq.activePhase, "discharge") == 0) d = mainSeq.currentPercent;
        
        comm.sendStatus(mainSeq.remainingSec, w, l, c, d); 
    }
}