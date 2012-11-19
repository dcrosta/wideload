#ifndef WIDELOAD_LIST_H
#define WIDELOAD_LIST_H

typedef struct _node {
    void * data;
    struct _node * next;
} node;

typedef struct {
    node * head;
    node * tail;
    unsigned long length;
} list;

list* list_new();
void list_free(list* l, int freedata);
void list_push(list* l, node* n);

node* list_node_new(void* data);

#endif
