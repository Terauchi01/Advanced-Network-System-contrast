#include "server.h"

void init_rooms()
{
    for (int i = 0; i < MAX_ROOMS; i++)
    {
        rooms[i].id = -1;
        rooms[i].active = 0;
    }
}

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

void close_room(Room *room)
{
    if (!room || !room->active)
        return;
    printf("Closing room %d\n", room->id);
    room->active = 0;
    room->id = -1;
}