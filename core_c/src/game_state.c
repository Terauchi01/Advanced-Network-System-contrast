#include "contrast_c/game_state.h"
#include <string.h>

void game_state_reset(GameState* state) {
    board_reset(&state->board);
    state->to_move = PLAYER_BLACK;
    state->inv_black.black = 3;
    state->inv_black.gray = 1;
    state->inv_white.black = 3;
    state->inv_white.gray = 1;
}

Player game_state_current_player(const GameState* state) {
    return state->to_move;
}

Board* game_state_board(GameState* state) {
    return &state->board;
}

const Board* game_state_board_const(const GameState* state) {
    return &state->board;
}

TileInventory* game_state_inventory(GameState* state, Player player) {
    return (player == PLAYER_BLACK) ? &state->inv_black : &state->inv_white;
}

const TileInventory* game_state_inventory_const(const GameState* state, Player player) {
    return (player == PLAYER_BLACK) ? &state->inv_black : &state->inv_white;
}

void game_state_apply_move(GameState* state, const Move* move) {
    if (!board_in_bounds(move->sx, move->sy) || !board_in_bounds(move->dx, move->dy)) {
        return;
    }
    
    Player p = state->to_move;
    Cell* src = board_at(&state->board, move->sx, move->sy);
    Cell* dst = board_at(&state->board, move->dx, move->dy);
    
    /* 駒を移動 */
    dst->occupant = src->occupant;
    src->occupant = PLAYER_NONE;
    
    /* タイル配置 */
    if (move->place_tile && board_in_bounds(move->tx, move->ty)) {
        Cell* tile_cell = board_at(&state->board, move->tx, move->ty);
        if (tile_cell->tile == TILE_NONE && tile_cell->occupant == PLAYER_NONE) {
            tile_cell->tile = move->tile;
            TileInventory* inv = game_state_inventory(state, p);
            if (move->tile == TILE_BLACK && inv->black > 0) {
                inv->black--;
            } else if (move->tile == TILE_GRAY && inv->gray > 0) {
                inv->gray--;
            }
        }
    }
    
    /* 手番交代 */
    state->to_move = (state->to_move == PLAYER_BLACK) ? PLAYER_WHITE : PLAYER_BLACK;
}

uint64_t game_state_compute_hash(const GameState* state) {
    uint64_t seed = 1469598103934665603ULL;
    const Cell* cells = state->board.cells;
    
    for (int i = 0; i < BOARD_CELLS; i++) {
        seed ^= (uint64_t)cells[i].occupant;
        seed *= 1099511628211ULL;
        seed ^= (uint64_t)cells[i].tile;
        seed *= 1099511628211ULL;
    }
    
    seed ^= (uint64_t)state->to_move;
    seed *= 1099511628211ULL;
    
    return seed;
}
