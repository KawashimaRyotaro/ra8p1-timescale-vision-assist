#ifndef COLOR_CYCLER_H
#define COLOR_CYCLER_H

#include <math.h>

/* 円周率の定義（環境によって math.h にない場合のための保険） */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/**
 * @brief カラーサイクルの状態（アトラクタ）を保持する構造体
 */
typedef struct {
    float angle;  /* 現在の角度（ラジアン: 0.0 〜 2*M_PI） */
    float speed;  /* 毎サイクル変化する速度（推奨値: 0.01 〜 0.05 程度） */
} ColorCycler;

/**
 * @brief カラーサイクラーの初期化
 * @param cycler 初期化する構造体へのポインタ
 * @param speed  変化のスピード
 */
void color_cycler_init(ColorCycler *cycler, float speed);

/**
 * @brief なめらかなレインボーカラーを計算し、HEX(0x00RRGGBBUL)形式で返す
 * @param cycler 状態構造体へのポインタ
 * @return unsigned long 0x00RRGGBBUL 形式のカラー値
 */
unsigned long color_cycler_update(ColorCycler *cycler);

#endif /* COLOR_CYCLER_H */
