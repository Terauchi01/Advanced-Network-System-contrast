#ifndef CONTRAST_C_TYPES_H
#define CONTRAST_C_TYPES_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* プレイヤー */
typedef enum {
    PLAYER_NONE = 0,
    PLAYER_BLACK = 1,
    PLAYER_WHITE = 2
} Player;

/* タイルタイプ */
typedef enum {
    TILE_NONE = 0,
    TILE_BLACK = 1,
    TILE_GRAY = 2
} TileType;

/* 盤面サイズ */
#define BOARD_W 5
#define BOARD_H 5
#define BOARD_CELLS (BOARD_W * BOARD_H)

/* 最大合法手数 */
#define MAX_MOVES 2048

#ifdef __cplusplus
}
#endif

#endif /* CONTRAST_C_TYPES_H */
