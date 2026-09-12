# 最終アーキテクチャ方針（固定）

この文書はTRONプログラミングコンテスト提出版に向けた開発方針を固定する。
以後、明確な実測根拠がない限り、この境界を変更しない。

## 1. 最終システムの目的

最終提出版はディスプレイ表示を目的としない。

```text
Camera
  ↓
Frame Cache
  ↓
Temporal analysis / Difference
  ↓
Model scheduler
  ├─ Fast path  : lightweight detector
  └─ Slow path  : YOLOX-Tiny
  ↓
Detection result
  ├─ GVS control
  └─ Stereo / spatial audio
```

LCDはデバッグ専用であり、最終処理パイプラインの必須タスクにしない。

## 2. CameraとUSB replayの共通境界

CameraとUSB replayは、`Frame Cache`へ同じ形式・同じ意味のframeを書き込む。

```text
Camera source ───┐
                 ├─> Frame Cache -> processing pipeline
USB replay  ─────┘
```

USB replayは「カメラ入力の代替」であり、モデル入力テンソルの代替ではない。
したがって、USB datasetには原則としてpreprocess済みINT8 tensorを保存しない。

USB replayで再現すべきものは、
「カメラドライバ/DMAが1 frameをFrame Cacheへ書き終えた直後のメモリ内容」である。

## 3. Camera Frame Contract

最終カメラの出力仕様を次の項目で固定し、その仕様をUSB replayにも適用する。

- width
- height
- pixel format
- byte order
- stride
- frame size
- cropping / scalingの有無
- frame timestamp / frame_idの扱い

Camera Frame Contract確定後は、USB datasetを同一形式に変換する。

最終カメラが低解像度を直接出力できる場合は、MCUでの毎frame resizeを避けることを優先する。
候補の優先順位は、カメラ実機対応が確認できる範囲で次の通り。

1. 224x168 RGB565（4:3、224x224 YOLO入力へ固定paddingのみ）
2. 320x240 RGB565
3. 640x480 RGB565

224x224 INT8はreplay専用形式になるため、Camera Frame Contractには採用しない。

### 224x168 VIN実機確認

2026-09-12、EK-RA8P1 + OV5640 + MIPI CSI + VIN構成で、VIN runtime scalingに224x168を追加し、Live camera streamingがエラーなく動作することを確認した。

この結果から、`width=224`、`height=168`をCamera Frame Contractの第一候補として継続採用する。
ただし、Camera Frame Contractを完全確定する前に、VINがSDRAMへ書き込む実際のline strideとframe memory layoutを実機または生成設定で確認する。
USB replayはその実メモリ配置まで一致させる。

## 4. Preprocessの位置づけ

Preprocessは最終システムに残る処理として扱う。
USB replayだけで省略してはならない。

ただしCamera Frame ContractをYOLO入力へ近づけることで、
resize / bilinear interpolationなどの不要な処理を最終システム自体から排除する。

例：224x168 RGB565を採用できる場合

```text
224x168 RGB565
  ↓
RGB565 unpack / channel reorder / quantize
  ↓
上下固定padding
  ↓
224x224x3 INT8
```

## 5. Debug Display

Displayは`debug observer`としてのみ使用する。
Recognition pipelineをDisplayの完了やbbox描画の完了でblockingしない。

```text
Frame Cache ─────> Recognition pipeline
      │
      └──────────> Debug Display (optional)

Detection snapshot ─> Debug Display overlay (optional)
```

Displayは最新frameと利用可能な最新Detection snapshotを表示する。
完全なframe同期表示が必要な単体デバッグ時を除き、NPU終了待ちは行わない。

提出版ではDisplay taskを無効化できる構成にする。

## 6. CPUコア分担

CPU1をDisplay専用コアにはしない。
Displayは最終システムから消えるため、最終アーキテクチャ上の資源配分理由にならない。

CPU1の用途は、必要性を実測してから以下を候補とする。

- GVS timing / safety control
- audio / spatial-audio event scheduling
- IPC
- watchdog / safety state
- other real-time control

AI / image pipelineをCPU0 + Ethos-U55で構成することを基本とし、CPU1分担は実測で決める。

## 7. 評価モード

### Regression / algorithm evaluation

Recorded camera-equivalent framesをUSB replayし、同一Frame Cache以降の処理を評価する。
再現性のあるdatasetでaccuracy、recall、model-selection、temporal scoreを比較する。

### Performance evaluation

Display OFFで測定する。

測定対象：
- frame acquisition latency
- preprocessing latency
- model latency
- postprocessing latency
- end-to-end response latency
- throughput / processed FPS
- deadline miss
- CPU/NPU utilization

Display時間は最終性能指標に含めない。

### Final demonstration

Camera -> same Frame Cache -> same processing pipeline -> GVS / stereo-spatial audio

USB replayからCameraへ移行するとき、Frame Cache以降のコードを変更しないことを目標とする。

## 8. 現在の基準実測

YOLOX-Tiny baseline（640x480 RGB565入力）で確認済み：

- preprocessing: 約468 ms/frame
- RunModel: 約320 ms/frame
- Ethos-U55 `ethosu_invoke_v3`: 約148 ms/frame
- postprocess: 約1 ms/frame
- total processing: 約790 ms/frame
- observed throughput: 約1.28 FPS

320x240 RGB565 USB replayでも300/300 frame完走を確認済み：

- preprocessing: 約430 ms/frame
- RunModel: 約315 ms/frame
- Ethos-U55 `ethosu_invoke_v3`: 約139 ms/frame
- postprocess: 約0.9 ms/frame

入力を640x480から320x240へ下げるだけでは、現行float bilinear preprocessingは約8%しか短縮しなかった。
したがって、224x168をVINで直接生成し、resize自体を最終システムから削除する方針を優先する。

## 9. 直近の開発順序

1. 224x168 VIN出力のSDRAM line stride / frame layoutを確認
2. Camera Frame Contractを完全固定
3. USB datasetをCamera Frame Contractと同一メモリ配置へ変換
4. `frame_source_usb` / `frame_source_camera`を同一Frame Cache APIへ接続
5. Displayをoptional debug observerとして維持
6. 224x168 Camera Contract専用preprocessを実装・計測
7. YOLO-Fastestを統合
8. Difference + adaptive model scheduler
9. GVS / stereo-spatial audio output
10. Display OFFで最終性能評価、Cameraで最終実証

この順序を以後の基準とする。
