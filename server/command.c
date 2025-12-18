#include "server.h"

int parse_coord(const char *str, int *x, int *y)
{
    if (!str || strlen(str) < 2)
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

void process_lobby_command(int client_idx, char *buffer)
{
    char cmd[10] = {0};
    int room_id = -1;

    int parsed = sscanf(buffer, "%9s", cmd);
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
        char list_buf[BUF_SIZE] = "Active Rooms:\n";
        int found = 0;

        for (int i = 0; i < MAX_CLIENTS; i++)
        {
            if (clients[i].state == STATE_WAITING)
            {
                char line[64];
                sprintf(line, "- Room %d (Waiting)\n", clients[i].room_id);
                if (strlen(list_buf) + strlen(line) < BUF_SIZE - 1)
                {
                    strcat(list_buf, line);
                }
                found = 1;
            }
        }
        for (int i = 0; i < MAX_ROOMS; i++)
        {
            if (rooms[i].active)
            {
                char line[64];
                sprintf(line, "- Room %d (Playing)\n", rooms[i].id);
                if (strlen(list_buf) + strlen(line) < BUF_SIZE - 1)
                {
                    strcat(list_buf, line);
                }
                found = 1;
            }
        }
        if (!found)
        {
            strcat(list_buf, "(None)\n");
        }
        send_msg(clients[client_idx].fd, list_buf);
    }
    else if (strcmp(cmd, "CREATE") == 0)
    {
        if (sscanf(buffer, "%*s %d", &room_id) == 1)
        {
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
                clients[client_idx].state = STATE_WAITING;
                clients[client_idx].room_id = room_id;
                clients[client_idx].player_color = PLAYER_BLACK;
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
                Room *room = get_free_room();
                if (room == NULL)
                {
                    send_msg(clients[client_idx].fd, "Error: Server room capacity full.\n");
                    return;
                }

                room->id = room_id;
                room->active = 1;
                room->black_idx = opponent_idx;
                room->white_idx = client_idx;
                game_state_reset(&room->game_state);

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
    else
    {
        send_msg(clients[client_idx].fd, "Unknown command.\n");
    }
}

void process_game_move(int client_idx, char *buffer)
{
    Room *room = get_room(clients[client_idx].room_id);
    if (!room)
    {
        send_msg(clients[client_idx].fd, "Error: Room error.\n");
        return;
    }

    Player current_turn = game_state_current_player(&room->game_state);
    if (current_turn != clients[client_idx].player_color)
    {
        send_msg(clients[client_idx].fd, "Error: Not your turn.\n");
        return;
    }

    char *args_ptr = strstr(buffer, " ");
    if (!args_ptr)
        return;
    args_ptr++;

    char src_str[10] = {0}, dst_str[10] = {0}, tile_part[10] = {0};

    char parse_buf[BUF_SIZE];
    strncpy(parse_buf, args_ptr, BUF_SIZE - 1);
    parse_buf[BUF_SIZE - 1] = '\0';

    for (int i = 0; parse_buf[i]; i++)
    {
        if (parse_buf[i] == ',')
            parse_buf[i] = ' ';
    }
    parse_buf[strcspn(parse_buf, "\n")] = 0;

    int count = sscanf(parse_buf, "%9s %9s %9s", src_str, dst_str, tile_part);

    int sx, sy, dx, dy;
    int place_tile = 0, tx = -1, ty = -1;
    TileType tile_type = TILE_NONE;

    if (count < 2)
    {
        send_msg(clients[client_idx].fd, "Error: Invalid format. Use 'a1,a2' or 'a1,a2 b1g'\n");
        return;
    }

    if (!parse_coord(src_str, &sx, &sy) || !parse_coord(dst_str, &dx, &dy))
    {
        send_msg(clients[client_idx].fd, "Error: Invalid coordinates.\n");
        return;
    }

    if (count >= 3 && strlen(tile_part) >= 3)
    {
        char t_coord[3] = {tile_part[0], tile_part[1], '\0'};
        char t_col = tolower(tile_part[2]);

        if (!parse_coord(t_coord, &tx, &ty))
        {
            send_msg(clients[client_idx].fd, "Error: Invalid tile coordinates.\n");
            return;
        }

        if (t_col == 'b')
            tile_type = TILE_BLACK;
        else if (t_col == 'g')
            tile_type = TILE_GRAY;
        else
        {
            send_msg(clients[client_idx].fd, "Error: Invalid tile color (b/g).\n");
            return;
        }
        place_tile = 1;
    }

    Move req_move;
    req_move.sx = sx;
    req_move.sy = sy;
    req_move.dx = dx;
    req_move.dy = dy;
    req_move.place_tile = place_tile;
    req_move.tx = tx;
    req_move.ty = ty;
    req_move.tile = tile_type;

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

    game_state_apply_move(&room->game_state, &req_move);

    int opponent_idx = (client_idx == room->black_idx) ? room->white_idx : room->black_idx;
    char move_msg[BUF_SIZE];

    sprintf(move_msg, "OPPONENT_MOVE %d %d %d %d %d %d %d %d\n",
            sx, sy, dx, dy, place_tile, tx, ty, (int)tile_type);
    send_msg(clients[opponent_idx].fd, move_msg);

    sprintf(move_msg, "YOUR_MOVE %d %d %d %d %d %d %d %d\n",
            sx, sy, dx, dy, place_tile, tx, ty, (int)tile_type);
    send_msg(clients[client_idx].fd, move_msg);

    if (rules_is_win(&room->game_state, clients[client_idx].player_color))
    {
        send_msg(clients[client_idx].fd, "WIN\n");
        send_msg(clients[opponent_idx].fd, "LOSE\n");
        close_room(room);

        clients[client_idx].state = STATE_LOBBY;
        clients[client_idx].room_id = -1;
        clients[client_idx].player_color = PLAYER_NONE;

        clients[opponent_idx].state = STATE_LOBBY;
        clients[opponent_idx].room_id = -1;
        clients[opponent_idx].player_color = PLAYER_NONE;
    }
    else
    {
        Player next_p = game_state_current_player(&room->game_state);
        if (rules_is_loss(&room->game_state, next_p))
        {
            send_msg(clients[client_idx].fd, "WIN (Opponent No Moves)\n");
            send_msg(clients[opponent_idx].fd, "LOSE (No Moves)\n");
            close_room(room);

            clients[client_idx].state = STATE_LOBBY;
            clients[client_idx].room_id = -1;
            clients[client_idx].player_color = PLAYER_NONE;

            clients[opponent_idx].state = STATE_LOBBY;
            clients[opponent_idx].room_id = -1;
            clients[opponent_idx].player_color = PLAYER_NONE;
        }
    }
}