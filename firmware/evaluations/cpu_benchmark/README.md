# EK-RA8P1 CPU対NPU性能比較

MLCommons Tinyの`ad01_int8`異常検知モデルをCPU（Central Processing Unit：汎用演算を行う
中央処理装置）だけで実行し、先に測定したNPU（Neural Processing Unit：ニューラル
ネットワーク演算専用回路）結果と比較する独立e² studioプロジェクトです。

## 比較条件

| 条件 | CPU版 | NPU版 |
|---|---:|---:|
| モデル | `ad01_int8` | `ad01_int8` |
| 入力 | 同じ固定640要素 | 同じ固定640要素 |
| ウォームアップ | 1回 | 1回 |
| 計測 | 100回 | 100回 |
| CPUクロック | 1 GHz | 1 GHz |
| 計測範囲 | 推論関数のみ | 推論関数のみ |

NPU版の実測平均値`107,680` CPU周期を比較基準として組み込んでいます。

CPU版はCMSIS-NN（Cortex Microcontroller Software Interface Standard Neural Network：
Arm Cortex-M向けニューラルネットワーク演算ライブラリ）で実行します。
`RM_ETHOSU_Open()`やNPU推論関数は呼びません。

## e² studioで実行する

1. `File` → `Import...`を開く
2. `General` → `Existing Projects into Workspace`を選ぶ
3. `firmware/evaluations/cpu_benchmark/`を指定する
4. `ruhmi_cpu_perf_eval_ek_ra8p1`を読み込む
5. `configuration.xml`を開いて`Generate Project Content`を押す
6. `Project` → `Build Project`を実行する
7. `Run` → `Debug Configurations...`を開く
8. `ruhmi_cpu_perf_eval_ek_ra8p1 Debug_Flat`を選んで`Debug`を押す
9. 最初の停止後に`Resume`（F8）を押す
10. 最後の`__BKPT(0)`で停止したら、Expressionsビューへ
    `g_cpu_benchmark_result`を追加する

GDB（GNU Debugger）は停止、変数表示、ステップ実行を行うデバッガです。

## 成功条件

```text
status = CPU_BENCHMARK_PASSED
output_mismatches = 0
output_max_abs_error = 0
```

## 比較する値

- `cpu_cycles_average`：CPU版の平均推論周期
- `inference_time_average_us`：CPU版の平均推論時間
- `npu_reference_cycles_average`：NPU版の平均推論周期。今回は107,680
- `npu_speedup_x1000`：NPUによる高速化率を1000倍した整数

例えば`npu_speedup_x1000 = 3500`なら、NPU版はCPU版より3.500倍高速です。

計算式は次の通りです。

```text
NPU高速化率 = CPU版の平均周期 ÷ NPU版の平均周期
```

## EK-RA8P1実測結果

```text
CPU版平均:       199,820周期（約199.820マイクロ秒）
NPU版平均:       107,680周期（約107.680マイクロ秒）
NPU高速化率:     1.855倍
出力不一致:      0/640
最大絶対誤差:    0
```

同一入力に対する出力が完全一致し、NPU版はCPU版より推論時間を約46.1%短縮しました。

## 注意

この比較は推論処理時間だけを対象にします。CPU版プロジェクトにも生成元ベースの
`rm_ethosu`構成が残りますが、CPU測定中にNPUは初期化も実行もされません。
そのため実行時間は比較できますが、最終バイナリ容量のCPU・NPU比較には使いません。

RUHMI（Robust Unified Heterogeneous Model Integration：ルネサスのAIモデル変換基盤）
2.6.0で`--cpu --ref-data`を指定して生成したコードを使用しています。

e² studio 2026-04.2、FSP（Flexible Software Package：ルネサスRA向けソフトウェア
基盤）6.5.0、LLVM（Low Level Virtual Machine：コンパイラ基盤）21.1.1でビルドし、
エラー0件を確認しています。RUHMIが生成したCPUコードを中心に警告が139件残って
いるため、実機測定後に警告内容を分類します。

参照元：[Renesas RUHMI Framework](https://github.com/renesas/ruhmi-framework-mcu)
