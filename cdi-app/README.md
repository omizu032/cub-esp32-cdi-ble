# CDI Tuning Dashboard

ESP32を搭載したカスタムCDIのためのワイヤレス設定アプリです。
Web Bluetooth APIを活用し、スマートフォンやPCのブラウザから直接ESP32と通信することで、専用アプリのインストールなしにCDIの設定ができます。

## ✨ 主な機能 (Key Features)

- **📊 リアルタイム状態表示 (Live Telemetry)**
  - エンジン回転数 (RPM) と進角 (Advance) を、コンビネーション・ダイアルゲージで表示。
  - エンジンの稼働状態（RUN / KILL / REV LIMIT）をモニタリング。
- **📈 点火マップエディタ (Ignition Map Editor)**
  - 1000〜9000 RPM (1000刻み) での進角を0.1度刻みで設定編集が可能。
  - 変更したマップはESP32のフラッシュメモリへ書き込み保存します。
- **📁 プロファイル管理 (Profile Management)**
  - 作成したマップをJSONファイルにして「街乗り用」「高回転パワー用」など名前を付けてブラウザ内にローカル保存。
  - エクスポート/インポートに対応し、セッティングデータの共有やバックアップが可能。
- **⚙️ セーフティ＆デバイス設定 (Configuration & Safety)**
  - **ハードウェア・レブリミッター**: 任意の回転数で物理的に点火をカットする安全装置付き。
  - **BLE OTAアップデート**: USBケーブル接続しなくてもワイヤレスでESP32のファームウェア(.bin)をアップデート可能。

## 🚀 システム要件 (Requirements)

- **デバイス**: Web Bluetooth APIをサポートするスマートフォン、タブレット、またはPC。
- **ブラウザ**:
  - Android: Google Chrome (推奨)
  - PC: Google Chrome, Microsoft Edge
  - iOS/iPadOS: Bluefy または WebBLE (※Appleの制限により標準SafariではWeb Bluetoothが動作しません)
- **ネットワーク**: Web Bluetoothのセキュリティ仕様上、本アプリは `https://` (SSL/TLS通信) または `localhost` でホストされている必要があります。

## Web UI開発環境の構築

動作確認やテスト運用のための環境の構築です

### 開発環境の前提条件

開発を行うPCには以下のインストールが必要です。

- **Node.js**: v18.0以上推奨 (LTSバージョン)
- **npm**: Node.jsに付属

### プロジェクトの初期化

※すでにプロジェクトが存在する場合は不要です。ゼロから作り直す場合の手順です。

```bash
# Viteを使ってVue 3プロジェクトを生成
npm create vite@latest cdi-app -- --template vue
```

Viteサーバが起動することを確認する

#### アプリファイルの上書き

プロジェクトフォルダ内の `src/App.vue` を置き換えます。
HMR（Hot Module Replacement）により、コードを保存した瞬間にブラウザの画面が更新されます。

開発サーバをCtrl+Cで停止した場合は

```bash
npm run dev
```

で再起動できます

- **アクセスURL:** `http://localhost:5173`
- **⚠️ Web Bluetoothの注意点:**
開発環境（`http://192.168.x.x` など）にスマホからアクセスしても、HTTPS環境ではないため Web Bluetooth API はブラウザにブロックされて起動しません。スマホで開発中の動作を見る場合は、PCとUSB接続して Chromeの「Port Forwarding（`chrome://inspect/#devices`）」を使用し、スマホ上の `http://localhost:5173` にルーティングさせる必要があります。

### ブラウザでの通信テスト

画面上の 「Connect to ESP32」 ボタンをクリックすると、ブラウザのポップアップ画面に「CDI-ESP32-S3」というデバイスが表示されるので、選択して「ペア設定」を押します。
接続が成功すると、画面上のステータスが切り替わります。このとき、ESP32側のシリアルモニタにも BLE App Connected! と表示されるか確認してください。

## Web サーバ上での稼働

屋外で実車にCDIを装着して動作確認をするための環境設定です

### 本番用のビルド

開発環境で確認出来たら、Webサーバ上へデプロイするためのファイル群を生成します

```bash
npm run build
```

### Webサーバへのデプロイ

本プロジェクトは静的ファイルのみで構成されているため、軽量なNginxコンテナ等を用いて配信するのが最適です。

- **サーバー側のディレクトリ構成**

```text
/your-server-dir/
 ├─ docker-compose.yml
 └─ dist/               <-- 開発PCでビルドしたdistフォルダをそのまま配置
     ├─ index.html
     └─ assets/
```

- **`docker-compose.yml` の設定例**

Nginxの公式軽量イメージを使用し、ホスト側の `dist` フォルダをコンテナ内のドキュメントルートに読み取り専用（`ro`）でマウントします。

```yaml

services:
  cdi-dashboard:
    image: nginx:alpine
    container_name: cdi-dashboard
    restart: unless-stopped
    ports:
      # ホストマシンのポート : コンテナのポート(80)
      - "8080:80"
    volumes:
      - ./dist:/usr/share/nginx/html:ro

```

- **コンテナの起動とSSL化**

1. サーバー上で `docker-compose up -d` を実行し、HTTPコンテナを立ち上げます。
2. リバースプロキシ（Nginx Proxy ManagerやTraefikなど）を設定し、取得済みのLet's Encrypt SSL証明書（`https://cdi.yourdomain.com`）へのアクセスを、上記コンテナの `8080` ポートへルーティングします。
3. これにより、スマホからHTTPS経由でセキュアにアクセス可能となり、Web Bluetooth APIが正常に動作します。

### 設定アプリのアップデート方法

アプリのUI改修や機能追加を行った場合のデプロイフローは非常にシンプルです。

1. 開発PCでコードを修正し、`npm run build` を実行。
2. 生成された新しい `dist` フォルダを、サーバー側の古い `dist` フォルダに上書き転送（置換）するだけ。
3. （Dockerコンテナの再起動は不要です。スマホでブラウザをリロードすれば最新版になります）。
