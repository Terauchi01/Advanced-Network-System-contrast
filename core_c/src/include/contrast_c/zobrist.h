#ifndef CONTRAST_C_ZOBRIST_H
#define CONTRAST_C_ZOBRIST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Zobrist テーブル初期化 */
void zobrist_init(void);

/* Zobrist ハッシュ取得 */
uint64_t zobrist_hash(void);

#ifdef __cplusplus
}
#endif

#endif /* CONTRAST_C_ZOBRIST_H */
