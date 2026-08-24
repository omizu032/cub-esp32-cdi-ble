# ホンダ スーパーカブ用 ESP32-S3 BLE デジタルCDI

ホンダ スーパーカブ90（HA02等）の横型エンジン向けに最適化された、ESP32-S3開発ボードをベースにしたプログラマブル・デジタル進角/遅角CDIのファームウェアです。

設定アプリとしてWeb Bluetooth APIとVite+Vue3を利用したPWA（Progressive Web App）により、アプリのインストール不要で、スマートフォンから安全かつ完全にワイヤレスで点火マップのセッティングとテレメトリ監視が可能です。

詳細は[こちら](https://zenn.dev/omizu032/articles/6b75e9b26bd7ec)にまとめてありますので、併せてごらんください

## ⚠️ 免責事項 (Disclaimer)

**本プロジェクトは自己責任で利用してください。**
点火タイミングの誤った設定は、エンジンの深刻な破損、重大な事故、または怪我につながる恐れがあります。本システムを使用したことによるいかなる損害についても、作成者は一切の責任を負いません。初めて実車で使用する際は、必ず安全なベースマップから開始し、フェイルセーフ機能の動作を確認してください。

## 主な特徴 (Key Features)

- **$\alpha-\beta$ フィルタの実装**
  単気筒エンジン特有の脈動（燃焼・排気ストロークの回転差）やノイズを吸収するため、整数型の定常状態推定フィルタを搭載。進角計算のジッター（ブレ）を低減しています。
- **デュアルコア分離**
  ESP32-S3のデュアルコアアーキテクチャを活用し、タスクを分離しています。
  - Core 1（エンジン制御専用）: パルス入力のハードウェア割り込みと、進角計算、サイリスタへの点火トリガーのみを専属処理。通信負荷による点火の遅延（ジッタ）を完全に排除。
  - Core 0（通信・UI専用）: BLE通信、NVS（不揮発性メモリ）へのマップ保存、フェイルセーフ監視などの非同期処理を担当。
- **Web Bluetooth API (PWA) によるワイヤレス・セッティング**
  - 完全ワイヤレス: ブラウザ（Chrome等）からHTTPS経由でアクセスし、BluetoothでCDI本体と直接通信します。
  - PWA対応: スマートフォンの「ホーム画面に追加」を行うことで、オフライン環境（山間部のツーリング先など）でもネイティブアプリのように爆速で起動・セッティングが可能。
  - リアルタイム・テレメトリ: RPM、現在の進角、キルスイッチのステータスをダッシュボードにリアルタイム表示。
  - 2Dマップエディタ: 画面上のグラフを指でドラッグ＆ドロップして、直感的に点火カーブを作成可能。


## 1. ハードウェア構成

詳細は[こちらの記事](https://zenn.dev/omizu032/articles/53a9f0207f47e5)を参照してください。

- **MCU:** ESP32-S3
- **Thyristor (SCR):** TYN625RG (高感度ゲート駆動対応)
- **Optocoupler:** LTV-817S-TA1-C (SMD)
- **Gate Control:** 2N7002 (N-ch MOSFET)
- **3.3V LDO:** AZ1117CH-3.3TRG1 (or 1117 compatible)
- **PCB Design:** スプリットGND構造 ＋ ビア・スティッチングによるシールド強化

## 2. ソフトウェア技術スタック (Software Stack)

- **デバイス側 (Firmware):** C/C++ (ESP-IDF / Arduino core for ESP32), FreeRTOS
- **クライアント側 (Web UI):** Vite, Vue 3, Web Bluetooth API

## 3. 使い方 (Getting Started)

### 3-1. ファームウェアの書き込み

1. Arduino IDE などで `cub-esp32-cdi-ble.ino` をESP32-S3 に書き込みます。

### 3-2. Web UI開発環境の構築


#### 3-2-1. 開発環境の前提条件 (Prerequisites)

開発を行うPCには以下のインストールが必要です。

- **Node.js**: v18.0以上推奨 (LTSバージョン)
- **npm**: Node.jsに付属

#### 3-2-2. プロジェクトの初期化 (Initialization)

※すでにプロジェクトが存在する場合は不要です。ゼロから作り直す場合の手順です。

```bash
# Viteを使ってVue 3プロジェクトを生成
npm create vite@latest cdi-app -- --template vue
```

Viteサーバが起動することを確認する

#### 3-2-3. アプリファイルの上書き

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

#### 3-3-4. ブラウザでの通信テスト

画面上の 「Connect to ESP32」 ボタンをクリックすると、ブラウザのポップアップ画面に「CDI-ESP32-S3」というデバイスが表示されるので、選択して「ペア設定」を押します。
接続が成功すると、画面上のステータスが切り替わります。このとき、ESP32側のシリアルモニタにも BLE App Connected! と表示されるか確認してください。

### 3-3. Web サーバ上での稼働

#### 3-3-1. 本番用のビルド

開発環境で確認出来たら、Webサーバ上へデプロイするためのファイル群を生成します

```bash
npm run build
```

#### 3-3-2. Webサーバへのデプロイ

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

#### 3-3-3. 今後のアップデート方法

アプリのUI改修や機能追加を行った場合のデプロイフローは非常にシンプルです。

1. 開発PCでコードを修正し、`npm run build` を実行。
2. 生成された新しい `dist` フォルダを、サーバー側の古い `dist` フォルダに上書き転送（置換）するだけ。
3. （Dockerコンテナの再起動は不要です。スマホでブラウザをリロードすれば最新版になります）。

