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

### 採用するContract

2026-09-12のEK-RA8P1 + OV5640 + MIPI CSI + VIN実機確認に基づき、第一実装のCamera Frame Contractを以下で固定する。

- active width: 224 pixels
- active height: 168 lines
- pixel format: RGB565
- bytes per pixel: 2
- active bytes per line: 448 bytes
- VIN input width: 1024 pixels
- VIN input height: 600 lines
- Frame Cache line stride: 2048 bytes
- Frame Cache span per image: 2048 x 168 = 344064 bytes
- active image payload: 224 x 168 x 2 = 75264 bytes
- scaling: VIN hardwareで1024x600 -> 224x168

VIN runtime scalingで224x168 Live camera streamingがエラーなく動作することを実機確認した。
また、Renesas例の`csi_check_image()`は縮小後のactive widthの後ろを、元のpreclip widthをstrideとしてskipする実装であるため、Frame Cacheはactive imageを2048-byte line pitchで扱う。

なお、デバッグ用`g_debug_vin_stride_bytes=2048`自体は`input_width*2`から算出した値であり、単独の独立測定ではない。Contract固定の根拠は、VIN設定値とRenesas例のline-skip実装、および224x168実機streamingの組合せとする。

USB datasetはファイル上でpaddingまで保持する必要はない。USB replay producerがtight-packed 224x168 RGB565を読み、Frame Cacheへ各行448 bytesずつ2048-byte strideで配置することで、Camera sourceと同じFrame Cache layoutを再現する。

224x224 INT8はreplay専用形式になるためCamera Frame Contractには採用しない。

## 4. Preprocessの位置づけ

Preprocessは最終システムに残る処理として扱う。
USB replayだけで省略してはならない。

Camera Frame Contractを224x168までVINで縮小することで、MCU上のresize / bilinear interpolationを最終システムから削除する。

```text
224x168 RGB565 (stride 2048 bytes)
  ↓
RGB565 unpack / channel reorder / quantize
  ↓
上下固定padding 28 lines + 28 lines
  ↓
224x224x3 INT8
```

preprocessはstride-awareにし、`source_width`と`source_stride_bytes`を分離して扱う。

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
したがって、224x168をVINで直接生成し、resize自体を最終システムから削除する方針を採用する。

## 9. 直近の開発順序

1. 224x168 tight-packed RGB565 USB datasetを作成
2. Frame Cache bufferを344064 bytes/frame、stride 2048 bytesとして扱えるよう変更
3. USB replay producerをrow-by-row copyに変更しCamera Frame Contractを再現
4. preprocessを224x168 + stride 2048専用のresizeなし実装へ変更・計測
5. `frame_source_camera`を同一Frame Cache APIへ接続
6. Displayをoptional debug observerとして維持
7. YOLO-Fastestを統合
8. Difference + adaptive model scheduler
9. GVS / stereo-spatial audio output
10. Display OFFで最終性能評価、Cameraで最終実証

この順序を以後の基準とする。
