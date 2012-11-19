#include <stdlib.h>

#include "list.h"

list* list_new()
{
    list* l = malloc(sizeof(list));
    l->head = NULL;
    l->tail = NULL;
    l->length = 0;
    return l;
}

void list_free(list* l, int freedata)
{
    node* n = l->head;
    node* p;
    while (n != NULL) {
        if (freedata && n->data != NULL)
            free(n->data);
        p = n;
        n = n->next;
        free(p);
    }
    free(l);
}

void list_push(list* l, node* n)
{
    if (l->tail == NULL) {
        // base case; set head and tail to the
        // new node, which is the first
        l->head = l->tail = n;
    } else {
        l->tail->next = n;
        l->tail = n;
    }
    l->length++;
}

node* list_node_new(void* data)
{
    node* n = malloc(sizeof(node));
    n->data = data;
    n->next = NULL;
    return n;
}
