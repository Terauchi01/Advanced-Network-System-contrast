#ifndef CONTRAST_C_BOARD_H
#define CONTRAST_C_BOARD_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* セル */
typedef struct {
    Player occupant;
    TileType tile;
} Cell;

/* 盤面 */
typedef struct {
    Cell cells[BOARD_CELLS];
} Board;

/* 盤面初期化 */
void board_reset(Board* board);

/* 座標が範囲内か */
int board_in_bounds(int x, int y);

/* セル取得 */
Cell* board_at(Board* board, int x, int y);

/* セル取得（const版） */
const Cell* board_at_const(const Board* board, int x, int y);

#ifdef __cplusplus
}
#endif

#endif /* CONTRAST_C_BOARD_H */
