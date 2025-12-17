#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netdb.h>
#include <ctype.h>

/* core_c のヘッダー */
#include "contrast_c/game_state.h"
#include "contrast_c/rules.h"
#include "contrast_c/move.h"
#include "contrast_c/types.h"

#define PORT 10000
#define BUF_SIZE 256

int sock_fd = -1;
GameState local_state;
int my_player_color = 0; // 0=Unknown, 1=Black, 2=White

/* 座標変換: "a1" -> x=0, y=0. 成功時1, 失敗時0 */
int parse_coord(const char *str, int *x, int *y)
{
    if (strlen(str) < 2)
        return 0;
    char col = tolower(str[0]);
    char row = str[1];

    if (col >= 'a' && col <= 'e')
        *x = col - 'a';
    else
        return 0;

    if (row >= '1' && row <= '5')
        *y = row - '1';
    else
        return 0;

    return 1;
}

/* 内部座標を文字列に変換: (0,0) -> "a1" */
void format_coord(int x, int y, char *buf)
{
    if (x >= 0 && x < 5 && y >= 0 && y < 5)
    {
        /* 修正: '1'+y だと文字コードになるため、y+1 を数値として渡す */
        sprintf(buf, "%c%d", 'a' + x, y + 1);
    }
    else
    {
        strcpy(buf, "??");
    }
}

/* 盤面表示関数 */
void print_board()
{
    const Board *b = game_state_board_const(&local_state);

    printf("\n    a  b  c  d  e\n");
    printf("  +--+--+--+--+--+\n");
    for (int y = 0; y < BOARD_H; y++)
    {
        printf("%d |", y + 1); // 行番号 1-5
        for (int x = 0; x < BOARD_W; x++)
        {
            const Cell *c = board_at_const(b, x, y);
            char piece = ' ';
            if (c->occupant == PLAYER_BLACK)
                piece = 'B';
            else if (c->occupant == PLAYER_WHITE)
                piece = 'W';

            char tile = ' ';
            if (c->tile == TILE_BLACK)
                tile = '#';
            else if (c->tile == TILE_GRAY)
                tile = '%';

            // 表示形式: [駒/タイル] 例: "B#" "W " " ."
            printf("%c%c|", piece, tile);
        }
        printf("\n  +--+--+--+--+--+\n");
    }

    // 在庫表示
    TileInventory *inv_b = game_state_inventory(&local_state, PLAYER_BLACK);
    TileInventory *inv_w = game_state_inventory(&local_state, PLAYER_WHITE);
    printf("Black(B) Inv: [#]%d [%%]%d\n", inv_b->black, inv_b->gray);
    printf("White(W) Inv: [#]%d [%%]%d\n", inv_w->black, inv_w->gray);

    // 手番表示
    Player cur = game_state_current_player(&local_state);
    printf("Turn: %s", (cur == PLAYER_BLACK) ? "BLACK" : "WHITE");
    if (my_player_color != 0)
    {
        printf(" (You are %s)\n", (my_player_color == PLAYER_BLACK) ? "BLACK" : "WHITE");
    }
    else
    {
        printf("\n");
    }
}

/* 合法手を列挙して表示（移動のみ） */
void print_legal_moves(const MoveList *moves)
{
    if (moves->size == 0)
    {
        printf("  (No legal moves - Pass or Loss)\n");
        return;
    }

    char s_buf[4], d_buf[4];
    printf("--- Valid Moves (Base Movement) ---\n");

    int count = 0;
    for (size_t i = 0; i < moves->size; i++)
    {
        const Move *m = &moves->moves[i];

        // タイル配置を含む手は表示しない
        if (m->place_tile)
            continue;

        format_coord(m->sx, m->sy, s_buf);
        format_coord(m->dx, m->dy, d_buf);

        printf("%s,%s", s_buf, d_buf);

        printf("\t");
        count++;
        if (count % 5 == 0)
            printf("\n");
    }
    if (count % 5 != 0)
        printf("\n");
    printf("(Optionally add tile: e.g. '... a1b')\n");
}

/* ユーザー入力を解析して Move を探す */
int parse_input_and_send(char *input, const MoveList *legal_moves)
{
    char src_str[10] = {0}, dst_str[10] = {0}, tile_part[10] = {0};
    int sx, sy, dx, dy;
    int place_tile = 0, tx = -1, ty = -1;
    TileType tile_type = TILE_NONE;

    for (int i = 0; input[i]; i++)
    {
        if (input[i] == ',')
            input[i] = ' ';
    }

    int count = sscanf(input, "%s %s %s", src_str, dst_str, tile_part);
    if (count < 2)
        return 0;

    if (!parse_coord(src_str, &sx, &sy) || !parse_coord(dst_str, &dx, &dy))
    {
        return 0;
    }

    if (count >= 3 && strlen(tile_part) >= 3)
    {
        char t_coord[3] = {tile_part[0], tile_part[1], '\0'};
        char t_col = tolower(tile_part[2]);

        if (!parse_coord(t_coord, &tx, &ty))
            return 0;

        if (t_col == 'b')
            tile_type = TILE_BLACK;
        else if (t_col == 'g')
            tile_type = TILE_GRAY;
        else
            return 0;

        place_tile = 1;
    }

    for (size_t i = 0; i < legal_moves->size; i++)
    {
        const Move *m = &legal_moves->moves[i];
        if (m->sx == sx && m->sy == sy && m->dx == dx && m->dy == dy)
        {
            if (m->place_tile == place_tile)
            {
                if (!place_tile)
                {
                    char buf[BUF_SIZE];
                    sprintf(buf, "MOVE %d %d %d %d 0 -1 -1 0\n", sx, sy, dx, dy);
                    write(sock_fd, buf, strlen(buf));

                    game_state_apply_move(&local_state, m);
                    print_board();
                    return 1;
                }
                else
                {
                    if (m->tx == tx && m->ty == ty && m->tile == tile_type)
                    {
                        char buf[BUF_SIZE];
                        sprintf(buf, "MOVE %d %d %d %d 1 %d %d %d\n",
                                sx, sy, dx, dy, tx, ty, (int)tile_type);
                        write(sock_fd, buf, strlen(buf));

                        game_state_apply_move(&local_state, m);
                        print_board();
                        return 1;
                    }
                }
            }
        }
    }
    return 0;
}

void prompt_move()
{
    if (game_state_current_player(&local_state) != my_player_color)
    {
        printf("Waiting for opponent...\n");
        return;
    }

    MoveList moves;
    rules_legal_moves(&local_state, &moves);
    print_legal_moves(&moves);

    if (moves.size > 0)
    {
        printf("Enter move (e.g. 'c5,c4'): ");
        fflush(stdout);
    }
}

int main(int argc, char *argv[])
{
    struct sockaddr_in serv_addr;
    struct hostent *server;
    fd_set read_fds;
    char buffer[BUF_SIZE];

    if (argc < 2)
    {
        fprintf(stderr, "Usage: %s <hostname>\n", argv[0]);
        exit(0);
    }

    sock_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (sock_fd < 0)
    {
        perror("socket");
        exit(1);
    }

    server = gethostbyname(argv[1]);
    if (server == NULL)
    {
        fprintf(stderr, "ERROR, no such host\n");
        exit(0);
    }

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    memcpy(&serv_addr.sin_addr.s_addr, server->h_addr_list[0], server->h_length);
    serv_addr.sin_port = htons(PORT);

    if (connect(sock_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("connect");
        exit(1);
    }

    printf("Connected. Commands: CREATE <id>, JOIN <id>\n");
    game_state_reset(&local_state);

    while (1)
    {
        FD_ZERO(&read_fds);
        FD_SET(0, &read_fds);
        FD_SET(sock_fd, &read_fds);

        if (select(sock_fd + 1, &read_fds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            break;
        }

        if (FD_ISSET(sock_fd, &read_fds))
        {
            memset(buffer, 0, BUF_SIZE);
            int n = read(sock_fd, buffer, BUF_SIZE - 1);
            if (n <= 0)
                break;

            if (strncmp(buffer, "OPPONENT_MOVE", 13) == 0)
            {
                int sx, sy, dx, dy, place, tx, ty, tile;
                sscanf(buffer, "OPPONENT_MOVE %d %d %d %d %d %d %d %d",
                       &sx, &sy, &dx, &dy, &place, &tx, &ty, &tile);
                Move m = {sx, sy, dx, dy, place, tx, ty, (TileType)tile};
                game_state_apply_move(&local_state, &m);
                printf("\nOpponent moved.\n");
                print_board();
                prompt_move();
            }
            else if (strstr(buffer, "You are BLACK"))
            {
                printf("%s", buffer);
                my_player_color = PLAYER_BLACK;
                game_state_reset(&local_state);
                print_board();
                prompt_move();
            }
            else if (strstr(buffer, "You are WHITE"))
            {
                printf("%s", buffer);
                my_player_color = PLAYER_WHITE;
                game_state_reset(&local_state);
                print_board();
                printf("Waiting for opponent...\n");
            }
            else if (strncmp(buffer, "WIN", 3) == 0 || strncmp(buffer, "LOSE", 4) == 0)
            {
                printf("\n%s\n", buffer);
                my_player_color = 0;
            }
            else if (strncmp(buffer, "OK", 2) != 0)
            {
                printf("%s", buffer);
            }
        }

        if (FD_ISSET(0, &read_fds))
        {
            memset(buffer, 0, BUF_SIZE);
            if (read(0, buffer, BUF_SIZE - 1) > 0)
            {
                buffer[strcspn(buffer, "\n")] = 0;

                int moved = 0;
                if (my_player_color != 0 && game_state_current_player(&local_state) == my_player_color)
                {
                    MoveList moves;
                    rules_legal_moves(&local_state, &moves);
                    if (parse_input_and_send(buffer, &moves))
                    {
                        moved = 1;
                    }
                }

                if (!moved)
                {
                    if (my_player_color != 0 && game_state_current_player(&local_state) == my_player_color)
                    {
                        printf("Invalid format or move.\n");
                        prompt_move();
                    }
                    else
                    {
                        strcat(buffer, "\n");
                        write(sock_fd, buffer, strlen(buffer));
                    }
                }
            }
        }
    }
    close(sock_fd);
    return 0;
}