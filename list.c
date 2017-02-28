#include <memory.h>
#include <malloc.h>
#include <stdio.h>
#include "list.h"

//Function Declarations
Node *node_create();

Node *list_get_last_node(List *list);

//Function Implementations
List *list_create() {
    List *list = malloc(sizeof(List));
    memset(list, 0, sizeof(List));
    list->size = 0;
    return list;
}

void list_add(List *list, void *value) {
    Node *node = node_create();

    node->value = value;

    if (list->size == 0) {
        list->head = node;
    } else {
        Node *last = list_get_last_node(list);
        last->next = node;
    }

    list->size++;
}

void *list_get(List *list, int index) {
    if (list->size <= index) {
        fprintf(stderr, "The passed in index is out of bounds (list_get).\n");
        return NULL;
    }

    Node *cur = list->head;

    for (int i = 1; i <= index; i++) {
        cur = cur->next;
    }

    return cur->value;
}

int list_contains(List *list, void *value, int (*equals)(void *, void *)) {
    if (list->size == 0) {
        return -1;
    }

    Node *cur = list->head;

    for (int i = 0; i < list->size; i++) {
        if (equals(cur->value, value)) {
            return i;
        } else {
            cur = cur->next;
        }
    }

    return -1;
}

void *list_remove(List *list, int index) {
    if (list->size <= index) {
        fprintf(stderr, "The passed in index is out of bounds (list_remove).\n");
        return NULL;
    }

    Node *prev = NULL;
    Node *cur = list->head;

    for (int i = 1; i <= index; i++) {
        prev = cur;
        cur = cur->next;
    }

    if (prev == NULL) {
        list->head = cur->next;
    } else {
        prev->next = cur->next;
    }

    list->size--;
    void *result = cur->value;
    free(cur);
    return result;
}

void *list_remove_value(List *list, void *value, int (*equals)(void *, void *)) {
    if (list->size == 0) {
        fprintf(stderr, "The list is empty (list_remove_value).\n");
        return NULL;
    }

    Node *prev = NULL;
    Node *cur = list->head;

    for (int i = 0; i < list->size; i++) {
        if (equals(cur->value, value)) {
            if (prev == NULL) {
                list->head = cur->next;
            } else {
                prev->next = cur->next;
            }

            list->size--;
            void *result = cur->value;
            free(cur);
            return result;
        }

        prev = cur;
        cur = cur->next;
    }

    return NULL;
}

void list_for_each(List *list, void (*apply)(void *)) {
    if(list->size == 0) {
        return;
    }

    Node *cur = list->head;

    for(int i = 0; i < list->size; i++) {
        if(cur->value) {
            apply(cur->value);
        }
    }
}

void list_free(List *list, void (*free_value)(void *)) {
    if (list->size == 0) {
        free(list);
        return;
    }

    Node *prev = NULL;
    Node *cur = list->head;

    for (int i = 0; i < list->size; i++) {
        if (cur->value != NULL && free_value != NULL)
            free_value(cur->value);

        prev = cur;
        cur = cur->next;
        free(prev);
    }

    free(list);
}

//"private" functions
Node *node_create() {
    Node *node = malloc(sizeof(Node));
    memset(node, 0, sizeof(Node));
    return node;
}

Node *list_get_last_node(List *list) {
    if (list->size == 0) {
        fprintf(stderr, "The list is empty (list_get_last_node)\n.");
        return NULL;
    }

    Node *cur = list->head;

    for (int i = 1; i < list->size; i++) {
        cur = cur->next;
    }

    return cur;
}
