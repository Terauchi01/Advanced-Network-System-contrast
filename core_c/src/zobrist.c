#include "./include/contrast_c/zobrist.h"
#include <stdlib.h>

static uint64_t g_zobrist_hash = 0;

void zobrist_init(void) {
    /* 簡易版: 固定シードで初期化 */
    g_zobrist_hash = 0x12345678ABCDEFULL;
}

uint64_t zobrist_hash(void) {
    return g_zobrist_hash;
}
