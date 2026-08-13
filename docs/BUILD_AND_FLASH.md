# ビルドと実機書込み

## 使用環境

- ボード：Renesas EK-RA8P1
- 開発環境：e2 studio 2026-04.2
- FSP（Flexible Software Package）：周辺回路の設定と制御コードを生成する仕組み、バージョン6.5.0
- 基本ソフトウェア：μT-Kernel 3.0 BSP2（Board Support Package：ボード用基本ソフトウェア）
- デバッガ：SEGGER J-Link

## プロジェクトの選択

| 目的 | ディレクトリ | e2 studioプロジェクト名 |
|---|---|---|
| デフォルト確認 | `firmware/ra8p1/` | `mtk3bsp2_ra8p1_ek` |
| カメラ表示評価 | `firmware/evaluations/camera_lcd/` | `mipi_csi_ek_ra8p1_ep` |

カメラ表示評価中は必ず`mipi_csi_ek_ra8p1_ep`を選択します。

評価プロジェクトは用途に合う公式サンプル、またはe2 studioの新規プロジェクト
作成機能から作ります。既存プロジェクトのメタデータを文字列置換して複製しません。

## e2 studioへ読み込む

1. `File` → `Import...`を開く
2. `General` → `Existing Projects into Workspace`を選ぶ
3. `Select root directory`へ`firmware/evaluations/camera_lcd/`を指定する
4. `mipi_csi_ek_ra8p1_ep`にチェックがあることを確認する
5. `Copy projects into workspace`は無効のまま`Finish`を押す

## 周辺回路設定を変更する

1. 対象評価プロジェクトの`configuration.xml`を開く
2. FSP Configuratorで必要な項目だけを変更する
3. Generate Project Contentを実行する
4. `ra_cfg/`と`ra_gen/`の変更内容を確認する
5. Applicationコードをビルドする

生成ファイルを直接書き換えません。

## 実機確認

1. `Project` → `Build Project`を実行する
2. `0 errors`で終了することを確認する
3. `Run` → `Debug Configurations...`を開く
4. `Renesas GDB Hardware Debugging`内の`mipi_csi_ek_ra8p1_ep Debug_Flat`を選ぶ
5. `Debug`を押し、書込みとデバッガ接続の完了を待つ
6. `main`で停止したら`Resume`（F8）を押す
7. 端末メニューで解像度`1`、続いてライブカメラ`1`を入力する
8. LCD表示と端末ログを確認し、結果を`docs/EXPERIMENTS.md`へ記録する

ソース確認だけでは、ビルド成功や実機動作済みとは扱いません。
