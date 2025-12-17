#include "contrast_c/rules.h"
#include <string.h>

/* 方向ベクトル */
static const int ORTHO[4][2] = {{1,0},{-1,0},{0,1},{0,-1}};
static const int DIAG[4][2] = {{1,1},{1,-1},{-1,1},{-1,-1}};
static const int ALL_8[8][2] = {{1,0},{-1,0},{0,1},{0,-1},{1,1},{1,-1},{-1,1},{-1,-1}};

void rules_legal_moves(const GameState* state, MoveList* out) {
    move_list_clear(out);
    
    const Board* b = game_state_board_const(state);
    Player p = game_state_current_player(state);
    
    /* 基本移動のみを格納する一時リスト */
    MoveList base_moves;
    move_list_clear(&base_moves);
    
    for (int y = 0; y < BOARD_H; y++) {
        for (int x = 0; x < BOARD_W; x++) {
            const Cell* cell = board_at_const(b, x, y);
            if (cell->occupant != p) continue;
            
            /* タイルによって移動方向を変える */
            const int (*dirs)[2] = NULL;
            int num_dirs = 0;
            
            if (cell->tile == TILE_NONE) {
                dirs = ORTHO;
                num_dirs = 4;
            } else if (cell->tile == TILE_BLACK) {
                dirs = DIAG;
                num_dirs = 4;
            } else {
                dirs = ALL_8;
                num_dirs = 8;
            }
            
            for (int i = 0; i < num_dirs; i++) {
                int dx = dirs[i][0];
                int dy = dirs[i][1];
                int tx = x + dx;
                int ty = y + dy;
                
                if (!board_in_bounds(tx, ty)) continue;
                
                const Cell* target = board_at_const(b, tx, ty);
                /* 敵駒でブロック */
                if (target->occupant != PLAYER_NONE && target->occupant != p) continue;
                
                /* 空きマスへの単純移動 */
                if (target->occupant == PLAYER_NONE) {
                    Move m = {x, y, tx, ty, 0, -1, -1, TILE_NONE};
                    move_list_push(&base_moves, &m);
                } else {
                    /* 自駒をジャンプ */
                    int jx = tx;
                    int jy = ty;
                    while (board_in_bounds(jx, jy)) {
                        const Cell* jcell = board_at_const(b, jx, jy);
                        if (jcell->occupant != p) break;
                        jx += dx;
                        jy += dy;
                    }
                    if (board_in_bounds(jx, jy)) {
                        const Cell* land = board_at_const(b, jx, jy);
                        if (land->occupant == PLAYER_NONE) {
                            Move m = {x, y, jx, jy, 0, -1, -1, TILE_NONE};
                            move_list_push(&base_moves, &m);
                        }
                    }
                }
            }
        }
    }
    
    /* タイル配置バリアント生成 */
    const TileInventory* inv = game_state_inventory_const(state, p);
    
    for (size_t i = 0; i < base_moves.size; i++) {
        const Move* base = &base_moves.moves[i];
        
        /* タイルなし */
        move_list_push(out, base);
        
        /* 黒タイル配置 */
        if (inv->black > 0) {
            for (int y = 0; y < BOARD_H; y++) {
                for (int x = 0; x < BOARD_W; x++) {
                    const Cell* cell = board_at_const(b, x, y);
                    if (cell->occupant == PLAYER_NONE && cell->tile == TILE_NONE) {
                        Move m = *base;
                        m.place_tile = 1;
                        m.tx = x;
                        m.ty = y;
                        m.tile = TILE_BLACK;
                        move_list_push(out, &m);
                    }
                }
            }
        }
        
        /* 灰タイル配置 */
        if (inv->gray > 0) {
            for (int y = 0; y < BOARD_H; y++) {
                for (int x = 0; x < BOARD_W; x++) {
                    const Cell* cell = board_at_const(b, x, y);
                    if (cell->occupant == PLAYER_NONE && cell->tile == TILE_NONE) {
                        Move m = *base;
                        m.place_tile = 1;
                        m.tx = x;
                        m.ty = y;
                        m.tile = TILE_GRAY;
                        move_list_push(out, &m);
                    }
                }
            }
        }
    }
}

int rules_is_win(const GameState* state, Player player) {
    const Board* b = game_state_board_const(state);
    int target_row = (player == PLAYER_BLACK) ? (BOARD_H - 1) : 0;
    
    for (int x = 0; x < BOARD_W; x++) {
        const Cell* cell = board_at_const(b, x, target_row);
        if (cell->occupant == player) {
            return 1;
        }
    }
    return 0;
}

int rules_is_loss(const GameState* state, Player player) {
    (void)player;
    MoveList moves;
    rules_legal_moves(state, &moves);
    return (moves.size == 0) ? 1 : 0;
}
