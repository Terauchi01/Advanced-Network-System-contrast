#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <signal.h> // SIGPIPE用

// サーバーのポート番号と最大クライアント数、バッファサイズを定義
#define PORT 10000
#define MAX_CLIENTS 10
#define BUF_SIZE 256
/*
クライアントの状態定数
STATE_NONE: 未使用（接続されていないスロット）
STATE_LOBBY: ロビーでコマンド入力待ち（初期状態）
STATE_WAITING: 部屋を作って対戦相手待ち
STATE_PLAYING: 対戦中
*/
#define STATE_NONE 0
#define STATE_LOBBY 1
#define STATE_WAITING 2
#define STATE_PLAYING 3
/*
クライアント情報を管理する構造体
fd: ソケットファイル記述子
state: 現在の状態 (STATE_*)
room_id: 所属する部屋ID
*/
typedef struct
{
    int fd;
    int state;
    int room_id;
} Client;

// 全クライアントの情報を保持する配列
Client clients[MAX_CLIENTS];
/*
クライアント配列の初期化
すべてのクライアントスロットを未使用状態にする
*/
void init_clients()
{
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        clients[i].fd = -1;
        clients[i].state = STATE_NONE;
        clients[i].room_id = -1;
    }
}

/*
メッセージ送信ヘルパー関数
指定されたファイル記述子にメッセージを送信する
writeエラーが発生した場合はperrorで出力するが、ここでは切断処理は行わない
(呼び出し元で切断検知を想定)
*/
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
/*
ロビーにいる全員にメッセージを送る
sender_idx: 送信元のクライアントインデックス
msg: 送信するメッセージ
送信元以外の、ロビーにいる全クライアントにメッセージを送信する
*/
void broadcast_lobby(int sender_idx, const char *msg)
{
    char buf[BUF_SIZE + 32];
    // メッセージのフォーマット: "Client <fd> says: <msg>"
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

/*
対戦相手にメッセージを送る
sender_idx: 送信元のクライアントインデックス
msg: 送信するメッセージ
同じ部屋IDを持ち、対戦中の相手を探してメッセージを送る
*/
void send_to_opponent(int sender_idx, const char *msg)
{
    int room_id = clients[sender_idx].room_id;
    for (int i = 0; i < MAX_CLIENTS; i++)
    {
        // 自分以外で、同じ部屋IDかつ対戦中の相手を探す
        if (i != sender_idx && clients[i].room_id == room_id && clients[i].state == STATE_PLAYING)
        {
            send_msg(clients[i].fd, msg);
            return; // 1vs1なので一人見つかれば終了
        }
    }
}
/*
新規クライアントの接続処理を行う関数
listen_fd: リッスン中のソケット
read_fds: select監視用のfd_set
max_fd: 現在の最大fd値へのポインタ
*/
void handle_new_connection(int listen_fd, fd_set *read_fds, int *max_fd)
{
    struct sockaddr_in cli_addr;
    socklen_t clilen = sizeof(cli_addr);
    int new_fd;

    // 新規接続を受け入れる
    if ((new_fd = accept(listen_fd, (struct sockaddr *)&cli_addr, &clilen)) < 0)
    {
        perror("accept");
    }
    else
    {
        printf("New connection from %s\n", inet_ntoa(cli_addr.sin_addr));
        int added = 0;
        // クライアント配列の空きスロットを探す
        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].fd == -1)
            {
                // クライアント情報を登録
                clients[i].fd = new_fd;
                clients[i].state = STATE_LOBBY; // 初期状態はロビー
                FD_SET(new_fd, read_fds);       // 監視対象に追加
                if (new_fd > *max_fd)
                    *max_fd = new_fd;
                // 歓迎メッセージとコマンド説明を送信
                send_msg(new_fd, "Welcome! Cmds: SAY <msg>, CREATE <id>, JOIN <id>\n");
                added = 1;
                break;
            }
        }
        // 満員の場合は拒否
        if (!added)
        {
            send_msg(new_fd, "Server full.\n");
            close(new_fd);
        }
    }
}
/*
クライアント切断時の処理を行う関数
client_idx: 切断されたクライアントのインデックス
read_fds: select監視用のfd_set
*/
void handle_disconnect(int client_idx, fd_set *read_fds)
{
    printf("Client %d disconnected.\n", clients[client_idx].fd);

    // 対戦中だった場合、相手に不戦勝を通知
    if (clients[client_idx].state == STATE_PLAYING)
    {
        send_to_opponent(client_idx, "Opponent disconnected. You Win!\n");
        // ※ 本来は相手の状態をロビーに戻すなどの処理が必要
    }

    // ソケットを閉じて監視から除外
    close(clients[client_idx].fd);
    FD_CLR(clients[client_idx].fd, read_fds);
    // クライアント情報を初期化
    clients[client_idx].fd = -1;
    clients[client_idx].state = STATE_NONE;
    clients[client_idx].room_id = -1;
}
/*
ロビーでのコマンド処理を行う関数
client_idx: コマンドを送信したクライアントのインデックス
buffer: 受信したデータ
*/
void process_lobby_command(int client_idx, char *buffer)
{
    char cmd[10];
    int room_id = -1;

    // 最初の単語をコマンドとして取得
    int parsed = sscanf(buffer, "%s", cmd);
    if (parsed < 1)
        return; // コマンドがなければ何もしない

    // SAYコマンド: ロビーチャット
    if (strcmp(cmd, "SAY") == 0)
    {
        // メッセージ部分を取得 (SAY 以降の文字列)
        char *msg_ptr = strstr(buffer, " ");
        if (msg_ptr)
        {
            broadcast_lobby(client_idx, msg_ptr + 1); // +1で空白をスキップ
        }
    }
    // CREATEコマンド: 部屋作成
    else if (strcmp(cmd, "CREATE") == 0)
    {
        if (sscanf(buffer, "%*s %d", &room_id) == 1)
        {
            // 部屋番号の重複チェック
            int exists = 0;
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
                // 部屋作成成功: 状態を待機中に変更
                clients[client_idx].state = STATE_WAITING;
                clients[client_idx].room_id = room_id;
                send_msg(clients[client_idx].fd, "Room created. Waiting...\n");
                printf("Client %d created Room %d\n", clients[client_idx].fd, room_id);
            }
        }
    }
    // JOINコマンド: 部屋への参加
    else if (strcmp(cmd, "JOIN") == 0)
    {
        if (sscanf(buffer, "%*s %d", &room_id) == 1)
        {
            int found = -1;
            // 指定された部屋IDで待機中の相手を探す
            for (int j = 0; j < MAX_CLIENTS; j++)
            {
                if (client_idx == j)
                    continue; // 自分自身は除外
                if (clients[j].state == STATE_WAITING && clients[j].room_id == room_id)
                {
                    found = j;
                    break;
                }
            }
            if (found != -1)
            {
                // マッチング成立
                clients[client_idx].state = STATE_PLAYING;
                clients[client_idx].room_id = room_id;

                // 相手の状態もPLAYINGに変更
                clients[found].state = STATE_PLAYING;

                // 両者に開始を通知
                send_msg(clients[client_idx].fd, "Matched! Start!\n");
                send_msg(clients[found].fd, "Opponent found! Start!\n");
                printf("Match: Client %d vs Client %d in Room %d\n", clients[found].fd, clients[client_idx].fd, room_id);
            }
            else
            {
                send_msg(clients[client_idx].fd, "Error: Room not found.\n");
            }
        }
    }
    else
    {
        send_msg(clients[client_idx].fd, "Unknown command.\n");
    }
}
/*
クライアントからのデータ受信処理を行う関数
client_idx: データを送信したクライアントのインデックス
read_fds: select監視用のfd_set
*/
void handle_client_data(int client_idx, fd_set *read_fds)
{
    char buffer[BUF_SIZE];
    int nbytes;

    memset(buffer, 0, BUF_SIZE);
    // データを読み込む
    nbytes = read(clients[client_idx].fd, buffer, BUF_SIZE - 1);

    // 切断検知 (readが0以下を返す)
    if (nbytes <= 0)
    {
        handle_disconnect(client_idx, read_fds);
    }
    else
    {
        // 対戦中はそのままデータを相手に転送
        if (clients[client_idx].state == STATE_PLAYING)
        {
            send_to_opponent(client_idx, buffer);
        }
        // ロビーにいる場合はコマンド処理
        else
        {
            process_lobby_command(client_idx, buffer);
        }
    }
}

int main()
{
    // SIGPIPEを無視 (対戦相手切断時のサーバーダウン防止)
    signal(SIGPIPE, SIG_IGN);

    int listen_fd, max_fd;
    struct sockaddr_in serv_addr;
    fd_set read_fds, temp_fds;

    // ソケット作成
    if ((listen_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    {
        perror("socket");
        exit(1);
    }

    // アドレス再利用設定 (サーバー再起動時のbindエラー回避)
    int opt = 1;
    setsockopt(listen_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // アドレス設定
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(PORT);

    // バインド
    if (bind(listen_fd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    // リッスン開始
    if (listen(listen_fd, 5) < 0)
    {
        perror("listen");
        exit(1);
    }

    // クライアント配列初期化
    init_clients();

    // select用設定の初期化
    FD_ZERO(&read_fds);
    FD_SET(listen_fd, &read_fds);
    max_fd = listen_fd;

    printf("Game Lobby Server started on port %d...\n", PORT);

    // メインループ
    while (1)
    {
        temp_fds = read_fds; // selectはセットを書き換えるためコピーを使う

        // 監視対象のファイル記述子に変化があるまで待機
        if (select(max_fd + 1, &temp_fds, NULL, NULL, NULL) < 0)
        {
            perror("select");
            break;
        }

        // A. 新規接続のチェック (リスニングソケットに変化あり)
        if (FD_ISSET(listen_fd, &temp_fds))
        {
            handle_new_connection(listen_fd, &read_fds, &max_fd);
        }

        // B. 既存クライアントからのデータ受信チェック
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