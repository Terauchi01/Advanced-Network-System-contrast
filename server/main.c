#include "server.h"

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

            clients[opponent_idx].state = STATE_LOBBY;
            clients[opponent_idx].room_id = -1;
            clients[opponent_idx].player_color = PLAYER_NONE;

            close_room(room);
        }
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
                clients[i].player_color = PLAYER_NONE;
                FD_SET(new_fd, read_fds);
                if (new_fd > *max_fd)
                    *max_fd = new_fd;
                send_msg(new_fd, "Welcome! Cmds: LIST, CREATE <id>, JOIN <id>, EXIT\n");
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
        buffer[nbytes] = '\0';

        if (clients[client_idx].state == STATE_PLAYING)
        {
            if (strncmp(buffer, "MOVE", 4) == 0)
            {
                process_game_move(client_idx, buffer);
            }
            else
            {
                send_msg(clients[client_idx].fd, "Unknown command in game. Use 'MOVE ...'\n");
            }
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
    init_rooms();

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