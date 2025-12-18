#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h>

/* core_c のヘッダーをインクルード */
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
    Player player_color; // PLAYER_BLACK or PLAYER_WHITE
} Client;

/* 部屋情報を管理する構造体 */
typedef struct
{
    int id;
    int black_idx; /* clients配列のインデックス */
    int white_idx; /* clients配列のインデックス */
    GameState game_state;
    int active;
} Room;

Client clients[MAX_CLIENTS];
Room rooms[MAX_ROOMS];

void init_clients()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].state = STATE_NONE;
        clients[i].room_id = -1;
        clients[i].player_color = PLAYER_NONE;
    }
}

void init_rooms()
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        rooms[i].id = -1;
        rooms[i].active = 0;
    }
}

/* 部屋IDからRoom構造体を取得 */
Room *get_room(int room_id)
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (rooms[i].active && rooms[i].id == room_id)
        {
            return &rooms[i];
        }
    }
    return NULL;
}

/* 空いているRoom構造体を取得 */
Room *get_free_room()
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        if (!rooms[i].active)
        {
            return &rooms[i];
        }
    }
    return NULL;
}

void send_msg(int fd, const char *msg)
{
    if (fd > 0)
    {
        if (write(fd, msg, strlen(msg)) < 0)
        {
            perror("write error");
        }
    }
}

void broadcast_lobby(int sender_idx, const char *msg)
{
    char buf[BUF_SIZE + 32];
    sprintf(buf, "Client %d says: %s", clients[sender_idx].fd, msg);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (i != sender_idx && clients[i].state == STATE_LOBBY)
        {
            send_msg(clients[i].fd, buf);
        }
    }
}

/* 部屋を解散させる */
void close_room(Room *room)
{
    if (!room || !room->active)
        return;

    printf("Closing room %d\n", room->id);

    // クライアントの状態をリセット（接続は切らずロビーに戻すか、切断処理に任せる）
    // ここでは単純に部屋を非アクティブにするのみ
    room->active = 0;
    room->id = -1;
}

void handle_disconnect(int client_idx, fd_set *read_fds)
{
    printf("Client %d disconnected.\n", clients[client_idx].fd);

    if (clients[client_idx].state == STATE_PLAYING)
    {
        Room *room = get_room(clients[client_idx].room_id);
        if (room)
        {
            int opponent_idx = (client_idx == room->black_idx) ? room->white_idx : room->black_idx;
            send_msg(clients[opponent_idx].fd, "Opponent disconnected. You Win!\n");
            // 相手をロビーに戻す
            clients[opponent_idx].state = STATE_LOBBY;
            clients[opponent_idx].room_id = -1;
            clients[opponent_idx].player_color = PLAYER_NONE;
            close_room(room);
        }
    }
    else if (clients[client_idx].state == STATE_WAITING)
    {
        // 待機中に抜けた場合、その部屋IDを使っている部屋があれば閉じる（本来は部屋リスト管理が必要）
        // ここでは簡易的に、その部屋IDを持つ有効な部屋を探して閉じる
        // （本来のコードではCREATE時にRoom確保していないため、厳密な管理はJOIN時に行われる）
    }

    close(clients[client_idx].fd);
    FD_CLR(clients[client_idx].fd, read_fds);
    clients[client_idx].fd = -1;
    clients[client_idx].state = STATE_NONE;
    clients[client_idx].room_id = -1;
    clients[client_idx].player_color = PLAYER_NONE;
}

void handle_new_connection(int listen_fd, fd_set *read_fds, int *max_fd)
{
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int new_fd;

    if ((new_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &clilen)) < 0)
    {
        perror("accept");
    }
    else
    {
        printf("New connection from %s\n", inet_ntoa(cli_addr.sin_addr));
        int added = 0;
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].fd == -1)
            {
                clients[i].fd = new_fd;
                clients[i].state = STATE_LOBBY;
                clients[i].room_id = -1;
                FD_SET(new_fd, read_fds);
                if (new_fd > *max_fd)
                    *max_fd = new_fd;
                send_msg(new_fd, "Welcome! Cmds: SAY <msg>, LIST, CREATE <id>, JOIN <id>, EXIT\n");
                added = 1;
                break;
            }
        }
        if (!added)
        {
            send_msg(new_fd, "Server full.\n");
            close(new_fd);
        }
    }
}

void process_lobby_command(int client_idx, char *buffer)
{
    char cmd[10];
    int room_id = -1;

    int parsed = sscanf(buffer, "%s", cmd);
    if (parsed < 1)
        return;

    if (strcmp(cmd, "SAY") == 0)
    {
        char *msg_ptr = strstr(buffer, " ");
        if (msg_ptr)
            broadcast_lobby(client_idx, msg_ptr + 1);
    }
    else if (strcmp(cmd, "LIST") == 0)
    {
        char room_list[BUF_SIZE * 2];
        strcpy(room_list, "=== Available Rooms ===\n");
        int found = 0;
        for (int j = 0; j < MAX_CLIENTS; j++)
        {
            if (clients[j].state == STATE_WAITING && clients[j].room_id != -1)
            {
                char temp[64];
                sprintf(temp, "Room %d (Waiting for opponent)\n", clients[j].room_id);
                strcat(room_list, temp);
                found = 1;
            }
        }
        if (!found)
        {
            strcat(room_list, "No rooms available. Create one with CREATE <id>\n");
        }
        send_msg(clients[client_idx].fd, room_list);
    }
    else if (strcmp(cmd, "CREATE") == 0)
    {
        if (sscanf(buffer, "%*s %d", &room_id) == 1)
        {
            int exists = 0;
            // 既存の待機中・対戦中の部屋IDと重複チェック
            for (int j = 0; j < MAX_CLIENTS; j++)
            {
                if (clients[j].state != STATE_NONE && clients[j].room_id == room_id)
                {
                    exists = 1;
                    break;
                }
            }
            if (exists)
            {
                send_msg(clients[client_idx].fd, "Error: Room exists.\n");
            }
            else
            {
                clients[client_idx].state = STATE_WAITING;
                clients[client_idx].room_id = room_id;
                clients[client_idx].player_color = PLAYER_BLACK; // 作成者を黒とする
                send_msg(clients[client_idx].fd, "Room created. Waiting... (You are BLACK)\n");
                printf("Client %d created Room %d\n", clients[client_idx].fd, room_id);
            }
        }
    }
    else if (strcmp(cmd, "JOIN") == 0)
    {
        if (sscanf(buffer, "%*s %d", &room_id) == 1)
        {
            int opponent_idx = -1;
            for (int j = 0; j < MAX_CLIENTS; j++)
            {
                if (client_idx == j)
                    continue;
                if (clients[j].state == STATE_WAITING && clients[j].room_id == room_id)
                {
                    opponent_idx = j;
                    break;
                }
            }
            if (opponent_idx != -1)
            {
                // 部屋用のスロット確保
                Room *room = get_free_room();
                if (room == NULL)
                {
                    send_msg(clients[client_idx].fd, "Error: Server room capacity full.\n");
                    return;
                }

                // 部屋の初期化
                room->id = room_id;
                room->active = 1;
                room->black_idx = opponent_idx;      // 待機していた人が黒
                room->white_idx = client_idx;        // 参加した人が白
                game_state_reset(&room->game_state); //

                // 両者の状態更新
                clients[opponent_idx].state = STATE_PLAYING;
                clients[client_idx].state = STATE_PLAYING;
                clients[client_idx].room_id = room_id;
                clients[client_idx].player_color = PLAYER_WHITE;

                send_msg(clients[client_idx].fd, "Matched! Start! (You are WHITE)\n");
                send_msg(clients[opponent_idx].fd, "Opponent found! Start! (You are BLACK)\n");
                printf("Match: Room %d started.\n", room_id);
            }
            else
            {
                send_msg(clients[client_idx].fd, "Error: Room not found.\n");
            }
        }
    }
    else if (strcmp(cmd, "EXIT") == 0)
    {
        send_msg(clients[client_idx].fd, "Goodbye!\n");
        // クライアント切断処理は handle_disconnect で行われる
        close(clients[client_idx].fd);
        clients[client_idx].fd = -1;
    }
    else
    {
        send_msg(clients[client_idx].fd, "Unknown command.\n");
    }
}

/* ゲームの手を処理する関数 */
void process_game_move(int client_idx, char *buffer)
{
    Room *room = get_room(clients[client_idx].room_id);
    if (!room)
    {
        send_msg(clients[client_idx].fd, "Error: Room error.\n");
        return;
    }

    // 手番チェック
    Player current_turn = game_state_current_player(&room->game_state);
    if (current_turn != clients[client_idx].player_color)
    {
        send_msg(clients[client_idx].fd, "Error: Not your turn.\n");
        return;
    }

    char cmd[10];
    int sx, sy, dx, dy, place, tx, ty, tile_val;

    // コマンド解析: MOVE sx sy dx dy place tx ty tile
    // place: 0 or 1, tile: 1(BLACK) or 2(GRAY)
    if (sscanf(buffer, "%s %d %d %d %d %d %d %d %d",
               cmd, &sx, &sy, &dx, &dy, &place, &tx, &ty, &tile_val) != 9)
    {
        // フォーマットが違う場合は無視、またはエラー通知
        return;
    }
    if (strcmp(cmd, "MOVE") != 0)
        return;

    Move req_move;
    req_move.sx = sx;
    req_move.sy = sy;
    req_move.dx = dx;
    req_move.dy = dy;
    req_move.place_tile = place;
    req_move.tx = tx;
    req_move.ty = ty;
    req_move.tile = (TileType)tile_val;

    // 合法手生成と検証
    MoveList legals;
    rules_legal_moves(&room->game_state, &legals);

    int is_legal = 0;
    for (size_t i = 0; i < legals.size; i++)
    {
        Move *m = &legals.moves[i];
        if (m->sx == req_move.sx && m->sy == req_move.sy &&
            m->dx == req_move.dx && m->dy == req_move.dy &&
            m->place_tile == req_move.place_tile)
        {

            if (m->place_tile)
            {
                if (m->tx == req_move.tx && m->ty == req_move.ty && m->tile == req_move.tile)
                {
                    is_legal = 1;
                }
            }
            else
            {
                is_legal = 1;
            }
        }
        if (is_legal)
            break;
    }

    if (!is_legal)
    {
        send_msg(clients[client_idx].fd, "Error: Illegal move.\n");
        return;
    }

    // 手を適用
    game_state_apply_move(&room->game_state, &req_move);

    // 相手へ通知 (MOVEコマンドをそのまま転送してUI更新を促す)
    int opponent_idx = (client_idx == room->black_idx) ? room->white_idx : room->black_idx;
    char move_msg[BUF_SIZE];
    sprintf(move_msg, "OPPONENT_MOVE %d %d %d %d %d %d %d %d\n",
            sx, sy, dx, dy, place, tx, ty, tile_val);
    send_msg(clients[opponent_idx].fd, move_msg);
    send_msg(clients[client_idx].fd, "OK\n");

    // 勝利判定
    if (rules_is_win(&room->game_state, clients[client_idx].player_color))
    {
        send_msg(clients[client_idx].fd, "WIN\n");
        send_msg(clients[opponent_idx].fd, "LOSE\n");
        close_room(room);
        // クライアント状態をロビーへ
        clients[client_idx].state = STATE_LOBBY;
        clients[opponent_idx].state = STATE_LOBBY;
    }
    else
    {
        // 相手の手番で合法手があるか（ステイルメイト/敗北判定）
        // rules_is_loss は「合法手がない場合」に真
        Player next_p = game_state_current_player(&room->game_state);
        if (rules_is_loss(&room->game_state, next_p))
        {
            send_msg(clients[client_idx].fd, "WIN (Opponent No Moves)\n");
            send_msg(clients[opponent_idx].fd, "LOSE (No Moves)\n");
            close_room(room);
            clients[client_idx].state = STATE_LOBBY;
            clients[opponent_idx].state = STATE_LOBBY;
        }
    }
}

void handle_client_data(int client_idx, fd_set *read_fds)
{
    char buffer[BUF_SIZE];
    int nbytes;

    memset(buffer, 0, BUF_SIZE);
    nbytes = read(clients[client_idx].fd, buffer, BUF_SIZE - 1);

    if (nbytes <= 0)
    {
        handle_disconnect(client_idx, read_fds);
    }
    else
    {
        if (clients[client_idx].state == STATE_PLAYING)
        {
            // MOVEコマンドの処理
            process_game_move(client_idx, buffer);
        }
        else
        {
            process_lobby_command(client_idx, buffer);
        }
    }
}

int main()
{
    signal(SIGPIPE, SIG_IGN);

    int listen_fd, max_fd;
    struct sockaddr_in serv_addr;
    fd_set read_fds, temp_fds;

    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }
    if (listen(listen_fd, 5) < 0)
    {
        perror("listen");
        exit(1);
    }

    init_clients();
    init_rooms(); // Room初期化

    FD_ZERO(&read_fds);
    FD_SET(listen_fd, &read_fds);
    max_fd = listen_fd;

    printf("Game Server started on port %d...\n", PORT);

    while (1)
    {
        temp_fds = read_fds;
        if (select(max_fd + 1, &temp_fds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            break;
        }

        if (FD_ISSET(listen_fd, &temp_fds))
        {
            handle_new_connection(listen_fd, &read_fds, &max_fd);
        }

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &temp_fds))
            {
                handle_client_data(i, &read_fds);
            }
        }
    }
    return 0;
}