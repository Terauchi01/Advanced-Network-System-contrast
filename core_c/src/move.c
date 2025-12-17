#include "contrast_c/move.h"

void move_list_clear(MoveList* list) {
    list->size = 0;
}

void move_list_push(MoveList* list, const Move* move) {
    if (list->size < MAX_MOVES) {
        list->moves[list->size++] = *move;
    }
}
