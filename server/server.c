#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h> // SIGPIPE用

#define PORT 10000
#define MAX_CLIENTS 10
#define BUF_SIZE 256

// クライアントの状態定数
#define STATE_NONE 0    // 未使用
#define STATE_LOBBY 1   // ロビーでコマンド入力待ち
#define STATE_WAITING 2 // 部屋を作って対戦相手待ち
#define STATE_PLAYING 3 // 対戦中

// クライアント情報を管理する構造体
typedef struct
{
    int fd;      // ソケットファイル記述子
    int state;   // 現在の状態
    int room_id; // 所属する部屋ID
} Client;

Client clients[MAX_CLIENTS];

// クライアント配列の初期化
void init_clients()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].state = STATE_NONE;
        clients[i].room_id = -1;
    }
}

// メッセージ送信ヘルパー関数
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

// ロビーにいる全員にメッセージを送る
void broadcast_lobby(int sender_idx, const char *msg)
{
    char buf[BUF_SIZE + 32];
    sprintf(buf, "Client %d says: %s", clients[sender_idx].fd, msg);
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // 自分以外かつロビーにいる人に送信
        if (i != sender_idx && clients[i].state == STATE_LOBBY)
        {
            send_msg(clients[i].fd, buf);
        }
    }
}

// 対戦相手にメッセージを送る
void send_to_opponent(int sender_idx, const char *msg)
{
    int room_id = clients[sender_idx].room_id;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        if (i != sender_idx && clients[i].room_id == room_id && clients[i].state == STATE_PLAYING)
        {
            send_msg(clients[i].fd, msg);
            return; // 1vs1なので一人見つかれば終了
        }
    }
}

int main()
{
    // SIGPIPEを無視 (対戦相手切断時のサーバーダウン防止)
    signal(SIGPIPE, SIG_IGN);

    int listen_fd, new_fd, max_fd, i, j;
    struct sockaddr_in serv_addr, cli_addr;
    socklen_t clilen;
    fd_set read_fds, temp_fds;
    char buffer[BUF_SIZE];
    int nbytes;

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
    FD_ZERO(&read_fds);
    FD_SET(listen_fd, &read_fds);
    max_fd = listen_fd;

    printf("Game Lobby Server started on port %d...\n", PORT);

    while (1)
    {
        temp_fds = read_fds;

        if (select(max_fd + 1, &temp_fds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            break;
        }

        // A. 新規接続
        if (FD_ISSET(listen_fd, &temp_fds))
        {
            clilen = sizeof(cli_addr);
            if ((new_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &clilen)) < 0)
            {
                perror("accept");
            }
            else
            {
                printf("New connection from %s\n", inet_ntoa(cli_addr.sin_addr));
                int added = 0;
                for (i = 0; i < MAX_CLIENTS; i++)
                {
                    if (clients[i].fd == -1)
                    {
                        clients[i].fd = new_fd;
                        clients[i].state = STATE_LOBBY;
                        FD_SET(new_fd, &read_fds);
                        if (new_fd > max_fd)
                            max_fd = new_fd;
                        send_msg(new_fd, "Welcome! Cmds: SAY <msg>, CREATE <id>, JOIN <id>\n");
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

        // B. データ受信
        for (i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].fd != -1 && FD_ISSET(clients[i].fd, &temp_fds))
            {
                memset(buffer, 0, BUF_SIZE);
                nbytes = read(clients[i].fd, buffer, BUF_SIZE - 1);

                // 切断検知
                if (nbytes <= 0)
                {
                    printf("Client %d disconnected.\n", clients[i].fd);

                    // 対戦相手への通知（不戦勝）
                    if (clients[i].state == STATE_PLAYING)
                    {
                        send_to_opponent(i, "Opponent disconnected. You Win!\n");
                        // 相手の状態をロビーに戻す処理等が必要ならここに追加
                    }

                    close(clients[i].fd);
                    FD_CLR(clients[i].fd, &read_fds);
                    clients[i].fd = -1;
                    clients[i].state = STATE_NONE;
                    clients[i].room_id = -1;
                }
                else
                {
                    // 対戦中はそのままデータを相手に転送
                    if (clients[i].state == STATE_PLAYING)
                    {
                        send_to_opponent(i, buffer);
                        continue;
                    }

                    // ロビーでのコマンド処理
                    char cmd[10];
                    char arg[BUF_SIZE];
                    int room_id = -1;

                    // 最初の単語をコマンドとして取得
                    int parsed = sscanf(buffer, "%s", cmd);
                    if (parsed < 1)
                        continue;

                    if (strcmp(cmd, "SAY") == 0)
                    {
                        // メッセージ部分を取得 (SAY 以降の文字列)
                        char *msg_ptr = strstr(buffer, " ");
                        if (msg_ptr)
                        {
                            broadcast_lobby(i, msg_ptr + 1); // +1で空白をスキップ
                        }
                    }
                    else if (strcmp(cmd, "CREATE") == 0)
                    {
                        if (sscanf(buffer, "%*s %d", &room_id) == 1)
                        {
                            // 重複チェック
                            int exists = 0;
                            for (j = 0; j < MAX_CLIENTS; j++)
                            {
                                if (clients[j].state != STATE_NONE && clients[j].room_id == room_id)
                                {
                                    exists = 1;
                                    break;
                                }
                            }
                            if (exists)
                            {
                                send_msg(clients[i].fd, "Error: Room exists.\n");
                            }
                            else
                            {
                                clients[i].state = STATE_WAITING;
                                clients[i].room_id = room_id;
                                send_msg(clients[i].fd, "Room created. Waiting...\n");
                                printf("Client %d created Room %d\n", clients[i].fd, room_id);
                            }
                        }
                    }
                    else if (strcmp(cmd, "JOIN") == 0)
                    {
                        if (sscanf(buffer, "%*s %d", &room_id) == 1)
                        {
                            int found = -1;
                            for (j = 0; j < MAX_CLIENTS; j++)
                            {
                                if (i == j)
                                    continue;
                                // 待機中の相手を探す
                                if (clients[j].state == STATE_WAITING && clients[j].room_id == room_id)
                                {
                                    found = j;
                                    break;
                                }
                            }
                            if (found != -1)
                            {
                                // マッチング成立
                                clients[i].state = STATE_PLAYING;
                                clients[i].room_id = room_id;

                                // ★修正: 相手の状態もPLAYINGに変更
                                clients[found].state = STATE_PLAYING;

                                send_msg(clients[i].fd, "Matched! Start!\n");
                                send_msg(clients[found].fd, "Opponent found! Start!\n");
                                printf("Match: Client %d vs Client %d in Room %d\n", clients[found].fd, clients[i].fd, room_id);
                            }
                            else
                            {
                                send_msg(clients[i].fd, "Error: Room not found.\n");
                            }
                        }
                    }
                    else
                    {
                        send_msg(clients[i].fd, "Unknown command.\n");
                    }
                }
            }
        }
    }
    return 0;
}