#ifndef UI_H
#define UI_H
#include <U8g2lib.h>
#include <Arduino.h>

// ジョイスティックから生成されるコマンドの種類を定義
enum Command { NONE, UP, DOWN, ENTER, BACK, LEFT, RIGHT };

class UI {
private:
    U8G2_SSD1306_128X64_NONAME_F_HW_I2C& u8g2; // U8g2ディスプレイオブジェクトへの参照
    uint8_t pinX, pinY, pinSW;                  // ジョイスティックの各ピン
    unsigned long lastAction = 0;               // チャタリング防止用の時刻保持変数

public:
    // コンストラクタ
    UI(U8G2_SSD1306_128X64_NONAME_F_HW_I2C& display, uint8_t x, uint8_t y, uint8_t sw) 
        : u8g2(display), pinX(x), pinY(y), pinSW(sw) {}

    // UIの初期化
    void setup() {
        u8g2.begin();
        pinMode(pinSW, INPUT_PULLUP);
    }

    // ジョイスティックの入力判定関数
    Command getCommand(bool ignoreWait = false) {
        if (!ignoreWait && millis() - lastAction < 200) return NONE;
        int x = analogRead(pinX); int y = analogRead(pinY);
        if (!digitalRead(pinSW)) { lastAction = millis(); return ENTER; }
        if (x < 500)  { lastAction = millis(); return RIGHT; }
        if (x > 3500) { lastAction = millis(); return LEFT; }
        if (y > 3500) { lastAction = millis(); return UP; }
        if (y < 500)  { lastAction = millis(); return DOWN; }
        return NONE;
    }

    // ★ 修正：縦並びメニューと「ページ番号（右上）」を正確に自動計算して描画する関数
    void drawMenu(const char* title, const char** items, int size, int cursor, bool* pumpStatus = nullptr) {
        u8g2.clearBuffer(); // 画面バッファをクリア
        
        // 1. タイトルバーの描画
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(0, 10, title);
        u8g2.drawHLine(0, 12, 128); // タイトル直下の区切り線

        // 2. ★ページ番号の動的自動計算ロジック
        // 現在のページ： (現在のカーソル位置 / 3) + 1
        // 総ページ数  ： ((全体の項目数 - 1) / 3) + 1
        int currentPage = (cursor / 3) + 1;
        int totalPage = ((size - 1) / 3) + 1;
        
        // 総ページ数が2ページ以上あるときだけ、画面右上（X=105, Y=10）に「1/2」のように表示する
        if (totalPage > 1) {
            char pageBuf[10];
            sprintf(pageBuf, "%d/%d", currentPage, totalPage);
            u8g2.drawStr(105, 10, pageBuf);
        }

        // 3. 3行ずつの項目描画処理
        int startIdx = (cursor / 3) * 3; // 描画を開始する配列のインデックス
        u8g2.setFont(u8g2_font_9x15_tr); // アイテム名は大きめのフォント
        
        for (int i = 0; i < 3; i++) {
            int idx = startIdx + i;
            if (idx >= size) break; // 登録サイズを超えたら描画終了
            
            int y = 32 + (i * 16); // 1行あたり16ピクセル間隔で配置
            
            // 選択中の行にカーソル「>」を描画
            if (idx == cursor) u8g2.drawStr(0, y, ">");
            
            // 項目名の描画
            u8g2.drawStr(12, y, items[idx]);
            
            // 【ポンプメニュー用】右端への ON/OFF トグルステータス表示
            // メニュー最後の「Main menu」には表示させないためのガード
            if (pumpStatus != nullptr && idx < (size - 1)) {
                u8g2.setFont(u8g2_font_6x12_tr); // ステータスは 6x12 フォント
                u8g2.drawStr(103, y - 2, pumpStatus[idx] ? "On" : "Off");
                u8g2.setFont(u8g2_font_9x15_tr); // フォントを戻す
            }
        }
        u8g2.sendBuffer(); // OLEDへ転送
    }

    // サーボ制御専用メニューの描画
    void drawServoMenu(int angle, int cursor) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_6x12_tr);
        u8g2.drawStr(0, 10, "Servo control");
        u8g2.drawHLine(0, 12, 128);

        char angBuf[20];
        sprintf(angBuf, "Pos: %d deg", angle);
        u8g2.drawStr(35, 28, angBuf);

        u8g2.setFont(u8g2_font_9x15_tr);
        if (cursor == 0) u8g2.drawStr(0, 48, ">");
        u8g2.drawStr(12, 48, "Front");

        int bw = u8g2.getStrWidth("Back");
        if (cursor == 1) u8g2.drawStr(128 - bw - 12, 48, ">");
        u8g2.drawStr(128 - bw, 48, "Back");

        int mw = u8g2.getStrWidth("Main menu");
        if (cursor == 2) u8g2.drawStr(128 - mw - 12, 63, ">");
        u8g2.drawStr(128 - mw, 63, "Main menu");
        u8g2.sendBuffer();
    }

    // システムメッセージ表示
    void drawSystemMessage(const char* m, bool big = false) {
        u8g2.clearBuffer();
        u8g2.setFont(big ? u8g2_font_ncenB12_tr : u8g2_font_9x15_tr);
        u8g2.drawStr((128 - u8g2.getStrWidth(m)) / 2, 40, m);
        u8g2.sendBuffer();
    }

    // 画面全消去
    void clearScreen() { u8g2.clearBuffer(); u8g2.sendBuffer(); }

    // プログレスバー描画
    void drawProgressBar(const char* m, int percent) {
        u8g2.clearBuffer();
        u8g2.setFont(u8g2_font_7x14_tr);
        u8g2.drawStr(0, 15, m);
        u8g2.drawFrame(0, 30, 128, 12);
        int barWidth = (max(0, min(100, percent)) * 124) / 100;
        u8g2.drawBox(2, 32, barWidth, 8);
        char buf[10]; sprintf(buf, "%3d%%", percent);
        u8g2.drawStr(90, 60, buf);
        u8g2.sendBuffer();
    }
};
#endif