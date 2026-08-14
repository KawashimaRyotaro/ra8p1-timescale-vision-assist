# 実験記録

## 記録形式

各実験で次を残します。

- 技術的な疑問
- 仮説
- 使用したボードとソフトウェア版
- 変更したプロジェクト
- 確認手順
- 測定結果
- 成功と失敗
- 学んだこと
- 設計への影響

## E001 μT-Kernelのタスク優先順位

状態：計画中

疑問：低い優先順位の高負荷処理中でも、緊急処理の応答時間を守れるか。

## E002 Ethos-U55による最初の推論

状態：NPU・CPU実機測定完了

Ethos-U55はRA8P1に搭載された、AI（Artificial Intelligence：人工知能）計算を
支援する回路です。

プロジェクト：`firmware/evaluations/npu_benchmark/`

疑問：既知の固定モデルをNPUで正しく実行できるか。また推論時間のばらつきはどの程度か。

使用する名称：

- NPU（Neural Processing Unit）：ニューラルネットワーク演算専用回路
- RUHMI（Robust Unified Heterogeneous Model Integration）：ルネサスのAIモデル変換基盤
- INT8：8ビット符号付き整数による量子化形式
- DWT（Data Watchpoint and Trace）：Cortex-M85内部のデバッグ・周期計測回路

実装済み内容：

- MLCommons Tiny `ad01_int8.tflite`をRUHMI 2.6.0でCコードへ変換した
- 10演算子すべてがEthos-U55へ割り当てられることを変換時に確認した
- 固定入力640要素と基準出力640要素をプロジェクトへ格納した
- ウォームアップ1回後に100回測定し、初回・最小・平均・最大周期を記録する
- NPU稼働周期数を性能監視回路から取得する
- 推論出力を基準出力と要素ごとに比較する
- FSP 6.5.0とLLVM 21.1.1へ移行し、Debugビルドを0エラーで完了した

NPU実機測定結果：

- 状態：成功
- CPUクロック：1,000,000,000 Hz
- 最小：107,679周期
- 平均：107,680周期、約107.680マイクロ秒
- 最大：107,740周期
- NPU稼働周期平均：53,277周期
- 出力不一致：0/640
- 最大絶対誤差：0
- 最大と最小の差：61周期、平均に対して約0.057%

CPU比較プロジェクト：`firmware/evaluations/cpu_benchmark/`

CPU実機測定結果：

- 状態：成功
- CPUクロック：1,000,000,000 Hz
- 最小：198,343周期
- 平均：199,820周期、約199.820マイクロ秒
- 最大：200,812周期
- 出力不一致：0/640
- 最大絶対誤差：0

CPU対NPU比較結果：

- NPU平均推論時間：107.680マイクロ秒
- CPU平均推論時間：199.820マイクロ秒
- NPUによる高速化：1.855倍
- NPUによる推論時間短縮：約46.1%、約92.14マイクロ秒
- CPU版とNPU版の出力は640要素すべて一致した

データシートおよび変換時予測との比較：

- RA8P1のNPUは最大500 MHz、8ビットMAC演算器256個、理論最大256 GOPS
- MAC（Multiply-Accumulate）は乗算と加算を一組として数える演算
- GOPS（Giga Operations Per Second）は1秒当たり10億演算
- 264,192 MACを理論ピークで処理する最短時間は約2.064マイクロ秒
- 実測107.680マイクロ秒の理論ピーク利用率は約1.92%、実効性能は約4.91 GOPS
- RUHMI予測は102.94マイクロ秒、51,470 NPU周期
- 実測は予測より時間で約4.6%、NPU稼働周期で約3.5%多く、予測とほぼ一致した
- RUHMI予測では内蔵フラッシュ読出し50,312周期がNPU演算40,248周期より長い

学んだこと：

- データシートの256 GOPSは全演算器を高い割合で使える場合の理論ピークである
- 今回の小規模モデルではNPU起動、同期、重み読出しの固定負担が相対的に大きい
- CPU比1.855倍だけではNPUの異常とは判断できず、モデル規模とメモリ律速を確認する必要がある
- 実測値がRUHMI予測へ約95%の水準で一致しているため、今回のNPU動作は妥当と判断する

参照：

- [RA8P1 Group Datasheet](https://www.renesas.com/en/document/dst/ra8p1-group-datasheet)
- [Renesas RA8P1 performance announcement](https://www.renesas.com/en/about/newsroom/renesas-sets-new-mcu-performance-bar-1-ghz-ra8p1-devices-ai-acceleration)
- [Building a Vision AI Application using the RA8P1 MCU with Ethos-U55 NPU](https://www.renesas.com/en/document/apn/building-vision-ai-application-using-ra8p1-mcu-ethos-u55-npu)

## E003 OV5640から液晶までの画像経路

状態：基本経路確認済み、画質評価中

プロジェクト：`firmware/evaluations/camera_lcd/`

疑問：OV5640の画像が、どの回路とメモリを通って液晶へ表示されるか。

使用する名称：

- MIPI CSI-2：カメラ画像を高速な直列信号で送る規格
- VIN（Video Input）：画像入力回路
- SDRAM（Synchronous Dynamic Random Access Memory）：外部大容量メモリ
- GLCDC（Graphics Liquid Crystal Display Controller）：液晶表示回路

現在の仮説：

```text
OV5640 → MIPI CSI-2 → VIN → 外部SDRAM → GLCDC → 液晶
```

現在の結果：

- Renesas公式EK-RA8P1 MIPI CSIサンプルを独立プロジェクトとして導入した
- FSP 6.5.0で周辺回路コードを生成した
- LLVM 21.1.1でDebugビルドし、ELF実行ファイルの生成を確認した
- デフォルトプロジェクト`firmware/ra8p1/`は変更していない
- EK-RA8P1へ書き込み、1024×600のOV5640ライブ映像をLCDへ表示できた
- カメラ初期化時のI²C転送中止を実機で切り分けた
- 公式例のI²C待機処理が、転送中止とタイムアウト時に永久待機する問題を修正した
- 現在の映像には黄色みと強いコントラストが見られる

学んだこと：

- OV5640の設定はI²Cで行い、画像本体はMIPI CSI-2で送られる
- VINがYCbCr 4:2:2をRGB565へ変換して外部SDRAMへ書き込む
- GLCDCが外部SDRAMの画像を継続的に読み出してLCDへ表示する
- LCDのバックライト点灯だけでは、カメラ取得や表示開始の成功を示さない
- 端末メニューが出ない場合は、メニューより前の初期化処理をCall Stackで調べる

設計への影響：

- この評価プロジェクトをカメラ入力とLCD表示の確認済み基準とする
- 画質調整とμT-Kernelへの移植は、この基準を保存した上で別段階として行う

## E004 カメラとLCDの色経路

状態：次に実施

疑問：黄色みと強いコントラストは、OV5640側とLCD表示側のどちらで生じているか。

用語：

- AWB（Auto White Balance）：白い物体が白く見えるように赤・緑・青の比率を調整する機能
- AEC（Auto Exposure Control）：明るさに応じて露光時間を調整する機能
- AGC（Auto Gain Control）：センサー信号の増幅率を自動調整する機能

確認手順：

1. 同じ室内照明とLCD設定を維持する
2. 端末で解像度`1`、カメラモード`2`を選び、カラーバーを表示する
3. 白、黒、赤、緑、青、黄、シアン、マゼンタの見え方を記録する
4. カラーバーで問題がなければ、解像度選択へ戻ってライブカメラを表示する
5. 同じ場面をスマートフォン画面などの基準と比較する

判断基準：

- カラーバーも黄色い：LCD出力、色順序、表示基板側を優先して調査する
- カラーバーは正常でライブ映像だけ黄色い：OV5640のAWBまたは色補正を調査する
- カラーバーの白黒も潰れる：VINの範囲変換またはGLCDC側を調査する
- カラーバーは正常でライブ映像だけ白飛び・黒つぶれする：OV5640のAECとAGCを調査する

この実験が終わるまで、AWB、AEC、AGC、VIN、GLCDCの設定を同時に変更しない。
