# RA8P1 Time-Scale Vision Assist

TRONプログラミングコンテスト2026へ向けた、歩行支援用エッジAI
（Artificial Intelligence：人工知能）システムです。

## 現在の状態

- `firmware/ra8p1/`：実機確認済みのデフォルトプログラム
- `firmware/evaluations/camera_lcd/`：OV5640カメラ表示の独立評価プロジェクト
- `firmware/evaluations/npu_benchmark/`：固定入力によるNPU推論の独立評価プロジェクト
- `firmware/evaluations/cpu_benchmark/`：同じモデルによるCPU対NPU性能比較プロジェクト
- デフォルトプログラムは評価コードから変更しない
- カメラ表示はRenesas公式MIPI CSIサンプルを基に実装済み
- FSP 6.5.0による生成とDebugビルドを確認済み
- EK-RA8P1で1024×600のOV5640ライブ映像表示を確認済み
- 黄色みと強いコントラストの原因を切り分け中

## 開発方針

完成品の統合と、技術を学ぶための評価を分離します。

```text
実機確認済みデフォルト
        |
        +-- 独立評価プロジェクト1
        +-- 独立評価プロジェクト2
        +-- 独立評価プロジェクト3
        |
        `-- 評価済み機能だけを最終システムへ統合
```

## 必読文書

- [プロジェクトの目的](docs/PROJECT_CONTEXT.md)
- [設計](docs/ARCHITECTURE.md)
- [開発順序](docs/ROADMAP.md)
- [設計判断](docs/DECISIONS.md)
- [ビルドと書込み](docs/BUILD_AND_FLASH.md)
- [実験記録](docs/EXPERIMENTS.md)

組込みファームウェアのビルドと実機確認はe2 studioで行います。

カメラ表示のBuild・Debug・操作手順は
[camera_lcd/README.md](firmware/evaluations/camera_lcd/README.md)を参照してください。
