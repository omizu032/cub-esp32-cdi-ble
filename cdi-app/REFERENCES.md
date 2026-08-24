# 技術リファレンス (Technical References)

本ドキュメントは、CDI Tuning Dashboardのシステムアーキテクチャおよび実装の技術的詳細を記載します。

## 1. システムアーキテクチャ

本システムは、Vue.js 3(Composition API)によるSPAフロントエンドと、ESP32(FreeRTOS/C++)によるバックエンドで構成されています。通信は全てWeb Bluetooth API (BLE GATT) を介して行われます。

*   **Frontend**: Vue 3, HTML5 SVG (描画), CSS Flexbox/Grid (レスポンシブ設計)
*   **Backend**: ESP32, FreeRTOS (Dual Core), gptimer (ハードウェアタイマー)
*   **Storage**: ESP32 Preferences (Flash), Web Browser LocalStorage

## 2. BLE 通信仕様 (GATT Profile)

| Service / Characteristic | UUID | Properties | Format / Description |
| :--- | :--- | :--- | :--- |
| **Main Service** | `...abcdef0` | - | - |
| Telemetry | `...abcdef1` | Read, Notify | 5 Bytes (uint16 RPM, int16 ADV_x10, uint8 Flags) |
| Ignition Map | `...abcdef2` | Read, Write | 18 Bytes (int16[9] array of ADV_x10) |
| Config (Rev Limit) | `...abcdef3` | Read, Write | 2 Bytes (uint16 RevLimit_RPM) |
| OTA Control | `...abcdef4` | Write | 1 Byte Cmd (0x01: Start + 4B Size, 0x02: End) |
| OTA Data | `...abcdef5` | Write Without Response | Binary Chunk (Max MTU, usually 200B ~ 500B) |
| Device Info | `...abcdef6` | Read | String (Version,BuildTimestamp) |

## 3. 主要なアルゴリズムと技術的解決策

### 3.1. ESP32 デュアルコア制御
*   **Core 1 (ハードウェア割り込み専用)**: ピックアップパルス(`pulsar_isr`)と点火タイマー(`on_ignition_timer_cb`)を処理。BLE通信の遅延によるエンストを防ぐため、点火ロジックは他タスクから完全に独立。
*   **Core 0 (バックグラウンド)**: BLE通信、RPMのフィルタリング処理(Alpha-Betaフィルタ)、ステータスの通知を担当。

### 3.2. ハードウェア・フェイルセーフ
*   **レブリミッター**: `pulsar_isr` 内で `current_rpm >= rev_limit_rpm` を検知した場合、点火タイマーをセットせずにスキップすることで物理的な点火カット(間引き)を実現。
*   **進角クランプ**: ソフトウェアのバグや通信エラーで異常値が送られてきても、ハードウェア側で `fdeg_x10 > 450 (45度)` の場合は強制的に45度にクリップする安全装置を実装。

### 3.3. モバイルブラウザ特有のバグ回避策 (CSS/HTML Hacks)
*   **Android Chromeの小数点バグ**: `<input type="number">` で `.toFixed(1)` された文字列をバインドすると値が消えるAndroid Chromeのバグを回避するため、`type="text"` + `inputmode="decimal"` を採用。
*   **SVG中心軸ズレ問題**: モバイルブラウザの `transform-origin` 解釈エラー（パーセント指定時のバグ）を防ぐため、コンビネーションゲージの回転中心を絶対ピクセル (`50px 50px`) および `0px 0px` で明示的に指定。
*   **ゲージ先端のオーバーラン**: `stroke-linecap: round` による描画はみ出し（半径分長くなる）を防ぐため、`butt` を採用して計算値にピクセルパーフェクトで追従。

### 3.4. BLE OTA (Over-The-Air) フロー
Web Bluetooth経由でESP32のアプリケーション領域 (`.ino.bin`) を直接書き換える。
1.  **Start**: `0x01` + 4バイトのファイルサイズを送信。ESP32が `Update.begin()` をコール。
2.  **Transfer**: 高速化のため `WriteWithoutResponse` でチャンクデータ(200B)をストリーミング。
3.  **End & Reboot**: `0x02` を送信。ESP32は `Update.end()` 成功後に `ESP.restart()` を実行。
4.  **Auto Reconnect**: アプリ側はエラー終了をハンドリングし、5秒後に再接続フローを自動実行。