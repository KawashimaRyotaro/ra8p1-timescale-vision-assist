# OV5640カメラ → LCD評価

このディレクトリは、デフォルトのμT-Kernel 3.0 BSP2プロジェクトを変更せずに
カメラ表示を確認する独立e2 studioプロジェクトです。

プロジェクト名は`mipi_csi_ek_ra8p1_ep`です。Renesas公式のEK-RA8P1向け
MIPI CSIサンプルを基にしています。MIPI CSIはMobile Industry Processor
Interface Camera Serial Interfaceの略で、カメラ画像を高速な直列信号で
受信する規格です。

## 実装されている処理

```text
OV5640
  ↓ MIPI CSI-2
RA8P1のMIPI受信回路
  ↓ YCbCr 4:2:2画像
VIN
  ↓ RGB565へ変換して書込み
外付けSDRAM上の画像バッファ
  ↓ 読出し
GLCDC
  ↓ RGB信号と同期信号
Parallel Graphics Expansion Board 1 v1
  ↓
1024×600 LCD
```

- VIN（Video Input）：画像の切出し、縮小、色変換、メモリ書込みを行う回路
- YCbCr：明るさと色差で色を表す形式
- RGB565：赤5ビット、緑6ビット、青5ビットの16ビット画素形式
- SDRAM（Synchronous Dynamic Random Access Memory）：画像を保持する外付けメモリ
- GLCDC（Graphics LCD Controller）：画像バッファを周期的に読み、LCD信号を出す回路
- I²C（Inter-Integrated Circuit）：OV5640のレジスタ設定に使う低速通信

アプリケーション入口は`src/hal_entry.c`の`hal_entry()`です。そこから
`mipi_csi_ep_entry()`を呼び、I²C、OV5640、VIN、GLCDCの順に初期化します。
この評価はカメラ経路を早く実機確認するためのFSP単体例であり、まだ
μT-Kernelタスクへ移植していません。

## 必要な接続

1. OV5640をEK-RA8P1のカメラコネクタ`J35`へ接続する
2. Parallel Graphics Expansion Boardの`J1`をEK-RA8P1の`J1`へ接続する
3. 両基板の1番ピン位置を合わせる
4. EK-RA8P1の`USB DEBUG`端子をPCへ接続する

`SW4-6`のMIPI選択はソフトウェアが制御します。

## e2 studioへ読み込む

1. `File` → `Import...`
2. `General` → `Existing Projects into Workspace`
3. `Select root directory`へこの`camera_lcd`ディレクトリを指定
4. `mipi_csi_ek_ra8p1_ep`を選択
5. `Copy projects into workspace`を無効にしたまま`Finish`

## GenerateとBuild

1. `configuration.xml`をダブルクリックする
2. FSP Configurator右上の`Generate Project Content`を押す
3. プロジェクトを右クリックして`Build Project`を実行する
4. Consoleの最後が`0 errors`であることを確認する
5. `Debug/mipi_csi_ek_ra8p1_ep.elf`が生成されたことを確認する

FSPはFlexible Software Packageの略で、周辺回路設定からドライバ設定コードを
生成するRenesasの仕組みです。`ra/`、`ra_cfg/`、`ra_gen/`は生成物なので、
直接編集しません。

確認済み環境ではe2 studio 2026-04.2、FSP 6.5.0、LLVM 21.1.1を使い、
Debugビルドまで完了しています。

## Debugと実行

1. Tera TermなどでJ-Linkの仮想COMポートを開く
2. 通信を`115200 bps`、データ8ビット、パリティなし、ストップ1ビット、
   フロー制御なしにする
3. `Run` → `Debug Configurations...`を開く
4. `Renesas GDB Hardware Debugging` →
   `mipi_csi_ek_ra8p1_ep Debug_Flat`を選ぶ
5. `Debug`を押す
6. `main`で停止したら`Resume`（F8）を押す
7. 端末の解像度メニューで`1`（1024×600）を入力する
8. カメラモードで`1`（Live camera）を入力する

この時点でLCDへ動画が表示されます。`2`はカメラではなくテストパターンです。

## 画面が出ない場合の最初の切り分け

1. 端末メニューが出ない：ファームウェア実行またはCOMポートの問題
2. テストパターンも出ない：LCD、GLCDC、SDRAM、基板接続の問題
3. テストパターンは出るがライブ映像だけ出ない：OV5640、FFCケーブル、I²C、
   MIPI受信の問題
4. 端末に`camera_open FAILED`：OV5640の制御通信またはクロックを確認する

FFCはFlexible Flat Cableの略で、カメラ接続用の薄い平型ケーブルです。

## 出典と検証状態

- 出典：Renesas `ra-fsp-examples`の`mipi_csi_ek_ra8p1_ep`
- 取得コミット：`01a411dfc2e9808f489070c780a554a5bead6714`
- ライセンス：各ソースのSPDX表記に従う
- Generate：確認済み
- Debug Build：確認済み
- EK-RA8P1への書込み：確認済み
- 1024×600のOV5640ライブ映像表示：確認済み
- 画質：黄色みと強いコントラストを確認、原因切り分け中

英語の公式説明は`RENESAS_MIPI_CSI_NOTES.md`に保存しています。

## 次の評価：カラーバー

現在の確認済み動作を保ったまま、端末で解像度`1`、カメラモード`2`を選びます。

- カラーバーも黄色い場合：LCD出力経路を調査する
- カラーバーが正常な場合：OV5640のAWB、AEC、AGCを調査する

AWBはAuto White Balanceの略で色温度を補正する機能、AECはAuto Exposure
Controlの略で露光時間を調整する機能、AGCはAuto Gain Controlの略で信号の
増幅率を調整する機能です。
