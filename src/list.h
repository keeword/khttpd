#ifndef _LIST_H_
#define _LIST_H_

#include "khttpd.h"
#include "parse.h"

int list_init(struct connector *head);

int list_add(struct connector *head, struct connector *entry);

int list_del(struct connector *entry);

struct connector *list_find(struct connector *head, int sock);

#endif // _LIST_H_
