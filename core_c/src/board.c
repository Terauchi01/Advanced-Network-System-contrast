#include "contrast_c/board.h"
#include <string.h>

void board_reset(Board* board) {
    /* 全セルをクリア */
    memset(board->cells, 0, sizeof(board->cells));
    
    /* 初期配置: 上段(y=0)に黒、下段(y=4)に白 */
    for (int x = 0; x < BOARD_W; x++) {
        board->cells[x].occupant = PLAYER_BLACK;  /* y=0 */
        board->cells[20 + x].occupant = PLAYER_WHITE;  /* y=4 */
    }
}

int board_in_bounds(int x, int y) {
    return (x >= 0 && x < BOARD_W && y >= 0 && y < BOARD_H);
}

Cell* board_at(Board* board, int x, int y) {
    return &board->cells[y * BOARD_W + x];
}

const Cell* board_at_const(const Board* board, int x, int y) {
    return &board->cells[y * BOARD_W + x];
}
