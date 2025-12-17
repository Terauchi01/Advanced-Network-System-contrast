#ifndef CONTRAST_C_GAME_STATE_H
#define CONTRAST_C_GAME_STATE_H

#include "board.h"
#include "move.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* タイル在庫 */
typedef struct {
    int black;
    int gray;
} TileInventory;

/* ゲーム状態 */
typedef struct {
    Board board;
    Player to_move;
    TileInventory inv_black;
    TileInventory inv_white;
    /* 履歴用のハッシュテーブルは簡易版では省略 */
} GameState;

/* ゲーム状態初期化 */
void game_state_reset(GameState* state);

/* 現在のプレイヤー取得 */
Player game_state_current_player(const GameState* state);

/* 盤面取得 */
Board* game_state_board(GameState* state);
const Board* game_state_board_const(const GameState* state);

/* 在庫取得 */
TileInventory* game_state_inventory(GameState* state, Player player);
const TileInventory* game_state_inventory_const(const GameState* state, Player player);

/* 指し手適用 */
void game_state_apply_move(GameState* state, const Move* move);

/* ハッシュ計算（簡易版） */
uint64_t game_state_compute_hash(const GameState* state);

#ifdef __cplusplus
}
#endif

#endif /* CONTRAST_C_GAME_STATE_H */
