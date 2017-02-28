#ifndef CHATSERVER_LIST_H
#define CHATSERVER_LIST_H

typedef struct list_node {
    struct list_node *next;
    void *value;
} Node;

typedef struct linked_list {
    Node *head;
    int size;
} List;

List *list_create();

void list_add(List *list, void *value);

void *list_get(List *list, int index);

int list_contains(List *list, void *value, int (*equals)(void *, void *));

void *list_remove(List *list, int index);

void *list_remove_value(List *list, void *value, int (*equals)(void *, void *));

void list_for_each(List *list, void (*apply)(void *));

void list_free(List *list, void (*free_value)(void *));

#endif //CHATSERVER_LIST_H
