# EK-RA8P1 NPU性能評価

カメラを使わず、固定入力でNPU（Neural Processing Unit：ニューラルネットワーク演算専用回路）の
推論時間と出力の正しさを測る独立e² studioプロジェクトです。
既存の`firmware/ra8p1/`とカメラ表示プロジェクトは変更しません。

## 評価内容

- モデル：MLCommons Tinyの`ad01_int8.tflite`異常検知モデル
- 量子化形式：INT8（8ビット符号付き整数）
- 入出力：640要素の固定ベクトル
- 実行：ウォームアップ1回、計測100回
- 計測：CPU周期数の初回値・最小値・平均値・最大値、NPU稼働周期数
- 正しさ：RUHMIが生成した基準出力640要素との完全一致

RUHMI（Robust Unified Heterogeneous Model Integration：ルネサスのAIモデル変換基盤）
2.6.0で変換した結果、10演算子すべてがArm Ethos-U55へ割り当てられています。
モデル当たりの演算量は264,192 MACです。MAC（Multiply-Accumulate）は、乗算と加算を
一組として数えるニューラルネットワークの基本演算です。

## データの流れ

```text
model_input_1（固定入力、内蔵フラッシュ）
    ↓ CPUが640バイトをコピー
NPU arena（作業用SRAM）
    ↓ RunModel(false)
Ethos-U55がcommand streamと重みを読み、推論
    ↓
NPU arena内の640バイト出力
    ↓ CPUが基準出力と比較
g_npu_benchmark_resultとRTTへ結果を保存
```

SRAM（Static Random Access Memory）はRA8P1内部の高速な読み書き用メモリです。
command streamは、Ethos-U55が実行する演算とデータ移動の命令列です。
RTT（Real-Time Transfer）は、SEGGER J-Link経由で文字列を表示するデバッグ通信方式です。

入力コピーは推論時間の外で行います。したがって測定値は、前処理やカメラ転送ではなく、
`RunModel(false)`による推論部分を表します。

## e² studioへ読み込む

1. e² studio 2026-04.2を起動する
2. `File` → `Import...`を開く
3. `General` → `Existing Projects into Workspace`を選ぶ
4. `Select root directory`へ`firmware/evaluations/npu_benchmark/`を指定する
5. `ruhmi_perf_eval_ek_ra8p1`だけにチェックする
6. `Copy projects into workspace`を無効のまま`Finish`を押す
7. `configuration.xml`を開き、FSP 6.5.0であることを確認する
8. `Generate Project Content`を押す

FSP（Flexible Software Package）は、RA8P1の周辺回路設定とドライバコードを生成する
ルネサスのソフトウェア基盤です。生成対象には`rm_ethosu`とArm Ethos-U Core Driverが含まれます。

## ビルドとデバッグ

1. Project Explorerで`ruhmi_perf_eval_ek_ra8p1`を選ぶ
2. `Project` → `Build Project`を実行する
3. Consoleの末尾が`Build Finished. 0 errors`であることを確認する
4. EK-RA8P1をUSBデバッグ端子でPCへ接続する
5. `Run` → `Debug Configurations...`を開く
6. `Renesas GDB Hardware Debugging`の
   `ruhmi_perf_eval_ek_ra8p1 Debug_Flat`を選ぶ
7. `Debug`を押し、ダウンロード完了後に`Resume`（F8）を押す
8. 最後の`__BKPT(0)`で停止したら、Expressionsビューへ
   `g_npu_benchmark_result`を追加する

GDB（GNU Debugger）は、停止、変数表示、ステップ実行を行うデバッガです。
J-Linkは、PCからEK-RA8P1へプログラムを書き込み、デバッグする装置です。

## 結果の読み方

`g_npu_benchmark_result.status`の値は次の意味です。

| 値 | 意味 |
|---:|---|
| 0 | 未実行 |
| 1 | 実行中 |
| 2 | 成功。基準出力と一致 |
| 3 | 推論出力が基準出力と不一致 |
| 4 | NPU初期化失敗 |
| 5 | NPU終了処理失敗 |

重要な測定値は次の通りです。

- `cpu_cycles_first`：ウォームアップ後の最初の計測値
- `cpu_cycles_min`：100回中の最小値
- `cpu_cycles_average`：100回の平均値
- `cpu_cycles_max`：100回中の最大値
- `inference_time_average_us`：平均推論時間。単位はマイクロ秒
- `npu_active_cycles_average`：Ethos-U55が実際に稼働した周期数の平均
- `output_mismatches`：基準出力と違った要素数。成功時は0
- `output_max_abs_error`：最大絶対誤差。成功時は0

RTTを有効にしたデバッグ環境では、同じ要約を文字列でも表示します。

## 実装ファイル

- `src/ruhmi_perf_eval/ruhmi_perf_eval.c`：初期化、反復計測、正解比較
- FSPの`rm_ethosu`：NPU制御とCPU・NPU間のキャッシュ整合処理
- `src/ruhmi_perf_eval/ruhmi_inference_code/`：RUHMIが生成したモデルCコード
- `src/ruhmi_perf_eval/utils/time_counter.c`：DWTによるCPU周期計測
- `configuration.xml`：FSP 6.5.0のNPU構成

DWT（Data Watchpoint and Trace）はCortex-M85内部のデバッグ・計測回路です。
この評価ではDWTの周期カウンタを推論時間の計測に使います。

CPUが入力を書いた後はデータキャッシュを主メモリへ反映し、NPU実行後はCPU側の古い
キャッシュを無効化します。この処理により、CPUとNPUが同じバッファ内容を参照できます。

## モデルの再生成

通常のビルドでは再生成不要です。再生成する場合は、Python 3.10と公式RUHMI 2.6.0を用意し、
公式リポジトリの`mcu_compile.py`を次のように実行します。

```powershell
python mcu_compile.py models_int8 deploy_output --npu --ref-data
```

`deploy_output/ad01_int8_NPU/deploy/build/MCU/compilation/src/`のうち、
`hal_entry.c`以外を`ruhmi_inference_code/`へ配置します。

参照元：

- [Renesas RUHMI Framework](https://github.com/renesas/ruhmi-framework-mcu)
- [Renesas公式NPU使用アプリケーションノート](https://www.renesas.com/ja/document/apn/using-ethos-u-npu-ra8-mcus)
- [MLCommons Tiny anomaly detection model](https://github.com/mlcommons/tiny/tree/master/benchmark/training/anomaly_detection)

## 現在の確認範囲

e² studio 2026-04.2、FSP 6.5.0、LLVM 21.1.1でDebugビルドし、
`ruhmi_perf_eval_ek_ra8p1.elf`の生成まで確認済みです。
実機上の性能値と出力一致は、EK-RA8P1でのデバッグ実行後に記録します。
