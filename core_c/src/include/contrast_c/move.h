#ifndef CONTRAST_C_MOVE_H
#define CONTRAST_C_MOVE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 指し手構造体 */
typedef struct {
    int sx;  /* 移動元 x */
    int sy;  /* 移動元 y */
    int dx;  /* 移動先 x */
    int dy;  /* 移動先 y */
    int place_tile;  /* タイル配置フラグ (0=なし, 1=あり) */
    int tx;  /* タイル配置先 x */
    int ty;  /* タイル配置先 y */
    TileType tile;  /* タイル種類 */
} Move;

/* 合法手リスト */
typedef struct {
    Move moves[MAX_MOVES];
    size_t size;
} MoveList;

/* MoveList 初期化 */
void move_list_clear(MoveList* list);

/* MoveList に追加 */
void move_list_push(MoveList* list, const Move* move);

#ifdef __cplusplus
}
#endif

#endif /* CONTRAST_C_MOVE_H */
