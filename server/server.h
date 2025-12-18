#ifndef SERVER_H
#define SERVER_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>
#include <ctype.h>

/* core_c のヘッダー */
#include "contrast_c/game_state.h"
#include "contrast_c/rules.h"
#include "contrast_c/move.h"
#include "contrast_c/types.h"

#define PORT 10000
#define MAX_CLIENTS 10
#define BUF_SIZE 256
#define MAX_ROOMS (MAX_CLIENTS / 2)

#define STATE_NONE 0
#define STATE_LOBBY 1
#define STATE_WAITING 2
#define STATE_PLAYING 3

typedef struct
{
    int fd;
    int state;
    int room_id;
    Player player_color;
} Client;

typedef struct
{
    int id;
    int black_idx;
    int white_idx;
    GameState game_state;
    int active;
} Room;

/* グローバル変数 (実体は main.c) */
extern Client clients[MAX_CLIENTS];
extern Room rooms[MAX_ROOMS];

/* 関数プロトタイプ */

/* network.c */
void send_msg(int fd, const char *msg);
void broadcast_lobby(int sender_idx, const char *msg);

/* room.c */
void init_rooms(void);
Room *get_room(int room_id);
Room *get_free_room(void);
void close_room(Room *room);

/* command.c */
int parse_coord(const char *str, int *x, int *y);
void process_lobby_command(int client_idx, char *buffer);
void process_game_move(int client_idx, char *buffer);

#endif