#ifndef CONTRAST_C_RULES_H
#define CONTRAST_C_RULES_H

#include "game_state.h"
#include "move.h"

#ifdef __cplusplus
extern "C" {
#endif

/* 合法手生成 */
void rules_legal_moves(const GameState* state, MoveList* out);

/* 勝利判定 */
int rules_is_win(const GameState* state, Player player);

/* 敗北判定（合法手なし） */
int rules_is_loss(const GameState* state, Player player);

#ifdef __cplusplus
}
#endif

#endif /* CONTRAST_C_RULES_H */
