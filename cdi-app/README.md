# CDI Tuning Dashboard

ESP32を搭載したカスタムCDI（キャパシター・ディスチャージ・イグニッション）のための、完全ワイヤレス・エンジンマネジメント・ダッシュボードです。
Web Bluetooth APIを活用し、スマートフォンやPCのブラウザから直接ESP32と通信することで、専用アプリのインストールなしに高度なECUチューニングを実現します。

## ✨ 主な機能 (Key Features)

- **📊 リアルタイム・テレメトリ (Live Telemetry)**
  - エンジン回転数 (RPM) と進角 (Advance) を、視認性の高いコンビネーション・ダイアルゲージで表示。
  - エンジンの稼働状態（RUN / KILL / REV LIMIT）をリアルタイムにモニタリング。
- **📈 インタラクティブ点火マップエディタ (Ignition Map Editor)**
  - 1000〜9000 RPM (1000刻み) の進角を、直感的なタッチドラッグ操作、または0.1度刻みの数値入力で編集可能。
  - 変更したマップはワンタップでESP32のフラッシュメモリへ即座に書き込み。
- **📁 プロファイル管理 (Profile Management)**
  - 作成したマップを「街乗り用」「高回転パワー用」など名前を付けてブラウザ内にローカル保存。
  - JSONファイルとしてのエクスポート/インポートに対応し、セッティングデータの共有やバックアップが可能。
- **⚙️ セーフティ＆デバイス設定 (Configuration & Safety)**
  - **ハードウェア・レブリミッター**: 任意の回転数で物理的に点火をカットする安全装置。
  - **BLE OTAアップデート**: ケースを開けることなく、ワイヤレスでESP32のファームウェア(.bin)をアップデート可能。

## 🚀 システム要件 (Requirements)

- **デバイス**: Web Bluetooth APIをサポートするスマートフォン、タブレット、またはPC。
- **ブラウザ**:
  - Android: Google Chrome (推奨)
  - PC: Google Chrome, Microsoft Edge
  - iOS/iPadOS: Bluefy または WebBLE (※Appleの制限により標準SafariではWeb Bluetoothが動作しません)
- **ネットワーク**: Web Bluetoothのセキュリティ仕様上、本アプリは `https://` (SSL/TLS通信) または `localhost` でホストされている必要があります。

## 💡 使用方法 (Usage)

1. バイクのイグニッションをONにし、ESP32を起動します。
2. ブラウザで本アプリ(HTTPS化されたURL)を開きます。
3. `Connect to ESP32` ボタンをタップし、デバイスリストから `CDI-ESP32-S3` 等を選択してペアリングします。
4. プルダウンメニューから各機能（Telemetry, Editor, Profile, Configuration）を切り替えて操作します。