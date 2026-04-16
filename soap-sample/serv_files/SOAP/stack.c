//#include <stdio.h>
//#include <stdlib.h>

//#include "stack.c"
typedef struct snode{
    int inf;
    struct snode * prev, * next;
}snode;
typedef struct stack{
    snode * up, * down;
    int level;
    int capacity;
}stack;
snode * create(int x){
    snode * p = (snode*)malloc(sizeof(snode));
    p->inf = x;
    p->prev = NULL;
    p->next = NULL;
    return p;
}

void dl_dealloc(snode *p){
    if(p != NULL)
        free(p);
}

stack createStack(int capacity){
    stack s;
    s.up = NULL;
    s.down = NULL;
    s.level = 0;
    s.capacity = capacity;
    return s;
}

void push(stack * s, int k){
    if(s->level > s->capacity){
        printf("Error! Limit exceeded\n");
    }
    snode * node = create(k);
    s->level ++; /* we add a new node */
    if(s->down == NULL){
        /* base node does not exist */
        s->down = node;
        return;
    }
    if(s->up == NULL){
        /* base node does exist, but top node does not */
        s->up = node;
        s->down->next = s->up;
        s->up->prev = s->down;
        return;
    }
    s->up->next = node;
    node->prev = s->up;
    s->up = node;
}

int pop(stack * s){
    int aux = s->up->inf;
    /* base case: 1 node in the stack */
    if(s->down->next == NULL){
        aux = s->down->inf;
       // free(s->down);
        return aux;
    }
    snode * tmp = s->up;
    s->up = s->up->prev;
    s->level --;
    dl_dealloc(tmp);
    return aux;
}

int top(stack * s){
    if(s->up != NULL){
        return s->up->inf;
    }
    return -1;
}


void traverse(stack s){
    snode * tmp = s.up;
    int curr_val;
    while(s.level > 0){
        curr_val = pop(&s);
        printf("%d\n",curr_val);
    }
}

int main() {
    stack s = createStack(10);
    for(int i=0;i<5;i++)
        push(&s, i);
    traverse(s);
    return 0;
}
