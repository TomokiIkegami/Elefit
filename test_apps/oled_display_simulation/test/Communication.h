#ifndef COMMUNICATION_H
#define COMMUNICATION_H
#include <Arduino.h>

class Communication {
private:
    String _command; // 解析後のコマンド文字列（"washing", "on_pump_12ch" など）を保持
    int _param1;     // 解析後のパラメータ1（速度や個別ポンプIDなど）を保持
    int _param2;     // 解析後のパラメータ2（12chポンプの正転/逆転フラグなど）を保持
    int _rxCount;    // 受信完了したコマンドの総数カウント（異常連打によるリセット判定用）
    String _buffer;  // 改行文字('\n')が届くまでの受信中データを一時的に蓄積するバッファ

public:
    // 初期化関数：シリアル通信を115200bpsで開始し、各変数をリセット
    void setup() {
        Serial.begin(115200);
        _command = "";
        _param1 = 0;
        _param2 = 0;
        _rxCount = 0;
        _buffer = "";
    }

    // 【毎ループ実行】シリアル受信バッファを監視し、1行分のデータが揃ったら解析へ回す関数
    // 戻り値: 1行分の受信・解析が完了した瞬間のみ true を返す
    bool update() {
        while (Serial.available()) {
            char c = Serial.read(); // バッファから1バイト読み出し
            if (c == '\n') { // 改行コードを検知＝1行データ受信完了
                parse(_buffer); // 蓄積された文字列の解析を実行
                _buffer = "";   // 次の受信に向けて一時バッファをクリア
                _rxCount++;     // 受信コマンド数をインクリメント
                return true;    // メイン側にコマンドが更新されたことを通知
            } else {
                _buffer += c;   // 改行が来るまでは文字列を後ろに結合していく
            }
        }
        return false; // データが未完成、または受信データがない場合は false
    }

    // Processingから届いた「コマンド,パラメータ1,パラメータ2」のCSV形式文字列を分解する関数
    // 例: "on_pump_12ch,100,1" を切り分ける
    void parse(String line) {
        line.trim(); // 文字列前後の不要な空白や改行の残骸を除去
        
        // カンマの位置を検索
        int firstComma = line.indexOf(',');                 // 1つ目のカンマの位置
        int secondComma = line.indexOf(',', firstComma + 1); // 2つ目のカンマの位置

        if (firstComma == -1) {
            // 【パターン1: カンマなし】 コマンド単体のみの場合（例: "washing"）
            _command = line;
            _param1 = 0;
            _param2 = 0;
        } else {
            // 【パターン2: カンマあり】 先頭から1つ目のカンマまでをコマンドとして抽出
            _command = line.substring(0, firstComma);
            
            if (secondComma == -1) {
                // 【パターン2-A: カンマが1つのみ】 パラメータ1までが存在する場合
                _param1 = line.substring(firstComma + 1).toInt(); // 文字列を数値に変換
                _param2 = 0;
            } else {
                // 【パターン2-B: カンマが2つ存在】 パラメータ1とパラメータ2をそれぞれ抽出
                _param1 = line.substring(firstComma + 1, secondComma).toInt(); // 1つ目と2つ目の間
                _param2 = line.substring(secondComma + 1).toInt();            // 2つ目の後ろすべて
            }
        }
    }

    // メインプログラム（main.ino）から解析結果を取得するためのゲッター関数群
    String getCommand() { return _command; }
    int getParam1() { return _param1; }
    int getParam2() { return _param2; } // elements[2] (12ch逆転フラグなど) に相当
    int getRxCount() { return _rxCount; }
    
    // 【重要】シーケンス中に溜まった先行入力をすべて破棄して誤作動を防ぐ関数
    void clearBuffer() {
        // ハードウェアシリアルリングバッファ内の未読データをすべて読み飛ばして空にする
        while (Serial.available() > 0) {
            Serial.read(); 
        }
        _buffer = "";      // 受信途中の文字列バッファもクリア
        _command = "";     // 保持していたコマンドも初期化してクリア
        _param1 = 0;
        _param2 = 0;
    }
    
    // Processingアプリ（Ver.4b）への進捗データ送信、およびシリアルモニタへのログ出力を同時に行う関数
    // rem: 残り秒数, w/l/c/d: 各フェーズの進捗率[%]
    void sendStatus(int rem, int w, int l, int c, int d) {
        // 1. Processingアプリ側のパース（要素数5の split(",")）に合わせた生データを出力（改行なし）
        Serial.print(rem); Serial.print(",");
        Serial.print(w);   Serial.print(",");
        Serial.print(l);   Serial.print(",");
        Serial.print(c);   Serial.print(",");
        Serial.print(d);   
        
        // 2. 人間がシリアルモニタでデバッグする用のテキスト情報を同じ行の右側に結合して出力（ここで改行）
        Serial.print("   -> [LOG] Rem:"); Serial.print(rem);
        Serial.print(" W:"); Serial.print(w);
        Serial.print(" L:"); Serial.print(l);
        Serial.print(" C:"); Serial.print(c);
        Serial.print(" D:"); Serial.println(d); // ここで1行の送信が完結する
    }
};
#endif