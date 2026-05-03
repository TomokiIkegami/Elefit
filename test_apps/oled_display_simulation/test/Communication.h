#ifndef COMMUNICATION_H
#define COMMUNICATION_H
#include <Arduino.h>

class Communication {
private:
    // 元コード通りの項目数（命令, 値1, 値2 の計3つ）
    static const int ELEMENTS_NUM = 3; 
    String elements[ELEMENTS_NUM];    // 受信データを格納する配列
    int rxCount = 0;                  // 受信フラグ用のカウンタ

public:
    // シリアル通信の開始
    void setup() {
        Serial.begin(115200);
    }

    /**
     * データ受信と解析
     * 「A,B,C」という形式の文字列を elements[0], [1], [2] に分解します
     */
    bool update() {
        if (Serial.available()) {
            // 改行コードまでを読み込み
            String line = Serial.readStringUntil('\n');
            line.trim();
            
            if (line.length() == 0) return false;

            int beginIndex = 0;
            // 指定された数（3つ）だけカンマで切り分ける
            for (int i = 0; i < ELEMENTS_NUM; i++) {
                if (i < ELEMENTS_NUM - 1) {
                    int endIndex = line.indexOf(',', beginIndex);
                    if (endIndex != -1) {
                        elements[i] = line.substring(beginIndex, endIndex);
                        beginIndex = endIndex + 1;
                    } else {
                        // カンマが足りない場合はそこまでで終了
                        elements[i] = line.substring(beginIndex);
                        for(int j = i + 1; j < ELEMENTS_NUM; j++) elements[j] = "";
                        break;
                    }
                } else {
                    // 最後の要素は残りの文字列すべて
                    elements[i] = line.substring(beginIndex);
                }
            }
            rxCount++;
            return true;
        }
        return false;
    }

    // 各要素を取得する関数
    String getCommand() { return elements[0]; }
    String getParam1()  { return elements[1]; }
    String getParam2()  { return elements[2]; }
    
    int getRxCount() { return rxCount; }
};
#endif