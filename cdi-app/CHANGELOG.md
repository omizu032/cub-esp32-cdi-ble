# Changelog

All notable changes to this project will be documented in this file.

## [1.0.1] - 2026-08-xx (Current Version)

### Added

- **BLE OTA Update**: ワイヤレスでのファームウェア（`.bin`）アップデート機能を実装。チャンクデータ転送と再起動後の自動再接続フローを確立。
- **Hardware Rev Limiter**: エンジン保護のため、任意の回転数で点火をカットする安全装置を `Configuration` に実装。
- **Device Information**: ESP32からファームウェアバージョンとビルド日時を取得して表示する機能を追加。
- **Responsive Layout**: プルダウンメニューによる画面切り替えと、スマートフォンの画面幅に合わせたCSSの徹底的なレスポンシブ化（`box-sizing: border-box`, `flex-wrap`等の導入）。

### Changed

- **Combination Dial Gauge**: RPMの針と進角のドーナツグラフを1つのSVGメーターに統合。
- **Dual Scale Gauge**: メーターの内側にRPMスケール、外側に進角（ADV）スケール（ピックアップ基準角27度の強調表示付き）を配置。
- **Numerical Readouts**: メーター内のデジタル数値をSVG外のHTML要素として分離し、スマホでの視認性とレイアウト崩れを改善。
- **Menu Label**: `Safety & Device` メニューを `Configuration` に名称変更。

### Fixed

- **Android Decimal Input Bug**: Android版Chromeにて小数点を含む数値入力が真っ白になるバグを、`type="text"` と `inputmode="decimal"` の併用により修正。
- **SVG Rotation Center Bug**: モバイルブラウザでのSVG針・バーの回転中心ズレを、`transform-origin` の絶対ピクセル指定により修正。
- **Gauge Linecap Overrun**: ドーナツグラフの描画において、指定角度より少しはみ出す問題を `stroke-linecap: butt` を用いて修正。
- **OTA Disconnect Error**: OTA完了時（ESP32再起動時）に発生する通信切断エラーを捕捉し、正常な自動再接続フローに遷移するよう修正。

## [1.0.0] - Initial Release

- Vue.js 3 と Web Bluetooth API を用いた初期開発。
- インタラクティブな点火マップエディタ（SVGドラッグ編集機能）の実装。
- リアルタイム・テレメトリの初期実装。
- LocalStorageを利用したプロファイル保存機能と、JSONエクスポート/インポート機能の実装。
