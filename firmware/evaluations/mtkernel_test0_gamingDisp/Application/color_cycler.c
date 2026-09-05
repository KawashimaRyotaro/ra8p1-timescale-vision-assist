#include "color_cycler.h"

void color_cycler_init(ColorCycler *cycler, float speed) {
    if (cycler != NULL) {
        cycler->angle = 0.0f;
        cycler->speed = speed;
    }
}

unsigned long color_cycler_update(ColorCycler *cycler) {
    if (cycler == NULL) return 0x00000000UL;

    // 1. 角度を更新し、2πを超えたらリセット（オーバーフロー・精度低下防止）
    cycler->angle += cycler->speed;
    if (cycler->angle >= 2.0f * M_PI) {
        cycler->angle -= 2.0f * M_PI;
    }

    // 2. R, G, B それぞれの波を120度（2/3 π）ずつずらしてサイン波を計算
    // sinfの戻り値 (-1.0 〜 1.0) を (0 〜 255) の範囲にスケーリング
    unsigned long r = (unsigned long)(127.5f + 127.5f * sinf(cycler->angle));
    unsigned long g = (unsigned long)(127.5f + 127.5f * sinf(cycler->angle + (2.0f * M_PI / 3.0f)));
    unsigned long b = (unsigned long)(127.5f + 127.5f * sinf(cycler->angle + (4.0f * M_PI / 3.0f)));

    // 3. ビットシフトで 0x00RRGGBBUL 形式に結合
    // 各チャンネルが 0xFF を超えないよう安全のために 0xFF でマスクします
    unsigned long hex_color = ((r & 0xFFUL) << 16) | 
                              ((g & 0xFFUL) << 8)  | 
                               (b & 0xFFUL);

    return hex_color;
}
