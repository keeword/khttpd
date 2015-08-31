#include <stdio.h>
#include <stdlib.h>
#include "list.h"

int list_init(struct connector *head)
{
        head->prev = head;
        head->next = head;
}

int list_add(struct connector *head, struct connector *entry)
{
        head->prev->next = entry;
        entry->prev      = head->prev;
        head->prev       = entry;
        entry->next      = head;

        return 0;
}

int list_del(struct connector *entry)
{
        entry->next->prev = entry->prev;
        entry->prev->next = entry->next;

        return 0;
}

struct connector *list_find(struct connector *head, int sock)
{
        struct connector *loop = head->next;
        for(; loop != head; loop = loop->next)
        {
                if (loop->connect_sock == sock) {
                        return loop;
                }
        }

        return NULL;
}
