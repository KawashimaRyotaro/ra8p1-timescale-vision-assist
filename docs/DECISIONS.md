# 設計判断

判断は永久的な規則ではありません。新しい測定結果が得られた場合は、
古い記録を残したまま更新します。

状態は「採用」「評価中」「不採用」「置換済み」のいずれかです。

## D001 リポジトリを開発記録の基準にする

状態：採用

ソース、設定、実験結果、判断理由をこのリポジトリに残します。

## D002 デフォルトプログラムを保護する

状態：採用

`firmware/ra8p1/`と`baseline-ra8p1-bsp2`タグを復旧点として保持します。

## D003 組込みビルドはe2 studioを基準にする

状態：採用

FSP（Flexible Software Package）の生成、ビルド、書込み、実機デバッグは
e2 studioで行います。FSPは周辺回路の設定と制御コードを生成する仕組みです。

## D004 アプリケーションと提供コードを分離する

状態：採用

独自コードは各プロジェクトの`Application/`へ置きます。

## D005 生成コードを手で書き換えない

状態：採用

周辺回路設定はFSP Configuratorで変更し、生成後の差分を確認します。

## D006 最初は一つのCPUコアを使う

状態：評価中

最初の統合はCortex-M85だけで行います。測定結果が必要性を示した場合は、
Cortex-M33との分担を評価します。

## D007 優先順位制御を研究仮説として扱う

状態：評価中

緊急処理を高い優先順位にすることで応答時間を守れるか、実測して判断します。

## D008 人体への電気刺激を初期開発条件にしない

状態：採用

GVS（Galvanic Vestibular Stimulation）は、電気前庭刺激を意味します。
初期評価では論理コマンド、計測出力、ダミー負荷を使用します。

## D009 評価ごとに独立プロジェクトを作る

状態：採用

評価コードは`firmware/evaluations/<評価名>/`へ置きます。周辺回路設定、
生成コード、ビルド結果を他の評価と分離し、デフォルトを変更しません。

## D010 カメラ入力は高速直列カメラ規格を第一候補にする

状態：カメラ表示評価で採用

MIPI CSI-2は、カメラ画像を高速な直列信号で送る規格です。Renesasの公式例と
付属OV5640の構成に合い、EK-RA8P1実機で1024×600のライブ映像表示を確認しました。
最終システムでも採用するかは、遅延とメモリ帯域の測定後に決めます。

## D011 USB replayはCamera Frame Cache境界を再現する

状態：採用

USB replayはpreprocess済みAI tensorを供給するのではなく、Camera/VINがFrame Cacheへ
書き込む画像と同じ形式・stride・byte order・frame sizeを再現します。
CameraとUSBはFrame Cache以降で同一のprocessing pipelineを使用します。

## D012 Displayはdebug observerとし、提出版の処理パスから外す

状態：採用

最終出力はGVSとstereo/spatial audioです。LCD表示は認識結果確認のためのdebug observer
としてのみ残し、性能評価と提出版では無効化します。Display完了待ちでRecognition pipelineを
blockしません。

## D013 Camera Frame Contractの第一実装はQVGA RGB565とする

状態：評価中

現行OV5640 + MIPI CSI + VINのRenesas例では、VINがYCbCr-422入力をRGB565へ変換し、
QVGA 320×240をSDRAMへ出力できることが確認できます。VGA 640×480も利用可能です。
したがってCamera/USB共通境界の第一実装は320×240 RGB565とします。

224×168 RGB565はYOLOX 224×224入力に対してresizeを不要にできる有力候補ですが、
現行Camera/VIN構成での実機出力をまだ確認していないため、現時点では固定しません。
320×240共通パイプラインを完成後、VIN設定で224×168出力が安定動作するかを独立評価し、
成功した場合のみCamera Frame Contractを置換します。
