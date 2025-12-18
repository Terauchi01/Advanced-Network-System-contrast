#include "server.h"

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