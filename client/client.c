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
#define BUF_SIZE 1024

int sock_fd = -1;
GameState local_state;
int my_player_color = 0; // 0=Unknown, 1=Black, 2=White

/* 内部座標を文字列に変換: (0,0) -> "a1" */
void format_coord(int x, int y, char *buf)
{
    if (x >= 0 && x < 5 && y >= 0 && y < 5)
    {
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

void prompt_move()
{
    if (my_player_color == 0)
        return;

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
        printf("Enter move (e.g. 'a1,a2' or 'a1,a2 b1g'): ");
        fflush(stdout);
    }
}

/* サーバーからの1行分のメッセージを処理する関数 */
void process_server_line(char *line)
{
    int sx, sy, dx, dy, place, tx, ty, tile;

    // 相手の手 (OPPONENT_MOVE) または 自分の受理された手 (YOUR_MOVE)
    if (strncmp(line, "OPPONENT_MOVE", 13) == 0 || strncmp(line, "YOUR_MOVE", 9) == 0)
    {
        char *params = strstr(line, " ");
        if (params)
        {
            sscanf(params, "%d %d %d %d %d %d %d %d",
                   &sx, &sy, &dx, &dy, &place, &tx, &ty, &tile);
            Move m = {sx, sy, dx, dy, place, tx, ty, (TileType)tile};
            game_state_apply_move(&local_state, &m);

            if (strncmp(line, "OPPONENT_MOVE", 13) == 0)
            {
                printf("\nOpponent moved.\n");
            }
            else
            {
                printf("\nMove accepted.\n");
            }
            print_board();
            prompt_move();
        }
    }
    else if (strstr(line, "You are BLACK"))
    {
        printf("%s\n", line);
        my_player_color = PLAYER_BLACK;
        game_state_reset(&local_state);
        print_board();
        prompt_move();
    }
    else if (strstr(line, "You are WHITE"))
    {
        printf("%s\n", line);
        my_player_color = PLAYER_WHITE;
        game_state_reset(&local_state);
        print_board();
        printf("Waiting for opponent...\n");
    }
    // 勝利判定の修正: "WIN" または "You Win!" (相手切断時) を検知
    else if (strncmp(line, "WIN", 3) == 0 || strstr(line, "You Win!"))
    {
        printf("\n%s\n", line); // 受信メッセージを表示 ("Opponent disconnected. You Win!")
        printf("!!! YOU WIN !!!\n");
        my_player_color = 0; // ゲーム終了状態へリセット
        printf("Returned to Lobby. (LIST, CREATE, JOIN, EXIT)\n");
    }
    else if (strncmp(line, "LOSE", 4) == 0)
    {
        printf("\n... You Lose ...\n");
        my_player_color = 0; // ゲーム終了状態へリセット
        printf("Returned to Lobby. (LIST, CREATE, JOIN, EXIT)\n");
    }
    else
    {
        // その他のメッセージ
        printf("%s\n", line);
        if (strncmp(line, "Error:", 6) == 0)
        {
            prompt_move();
        }
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

    printf("Connected. Commands: LIST, CREATE <id>, JOIN <id>, EXIT\n");
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

        /* サーバーからの受信 */
        if (FD_ISSET(sock_fd, &read_fds))
        {
            memset(buffer, 0, BUF_SIZE);
            int n = read(sock_fd, buffer, BUF_SIZE - 1);
            if (n <= 0)
                break;

            buffer[n] = '\0';
            char *line = strtok(buffer, "\n");
            while (line != NULL)
            {
                process_server_line(line);
                line = strtok(NULL, "\n");
            }
        }

        /* キーボード入力 */
        if (FD_ISSET(0, &read_fds))
        {
            memset(buffer, 0, BUF_SIZE);
            if (read(0, buffer, BUF_SIZE - 1) > 0)
            {
                buffer[strcspn(buffer, "\n")] = 0;

                // EXITコマンド
                if (strncmp(buffer, "EXIT", 4) == 0)
                {
                    printf("Exiting...\n");
                    close(sock_fd);
                    exit(0);
                }

                // ゲーム中かつ自分の手番ならMOVEコマンドとして送信
                if (my_player_color != 0 && game_state_current_player(&local_state) == my_player_color)
                {
                    char send_buf[BUF_SIZE];
                    sprintf(send_buf, "MOVE %s\n", buffer);
                    write(sock_fd, send_buf, strlen(send_buf));
                }
                else
                {
                    // それ以外（ロビー）はそのまま送信
                    strcat(buffer, "\n");
                    write(sock_fd, buffer, strlen(buffer));
                }
            }
        }
    }
    close(sock_fd);
    return 0;
}