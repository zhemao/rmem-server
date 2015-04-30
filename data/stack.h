#ifndef _STACK_H_
#define _STACK_H_

typedef struct stack* stack_p;

stack_p stack_create();
void stack_destroy(stack_p*);

void stack_push(stack_p, void*);
void* stack_pop(stack_p);

int stack_size(stack_p);

#endif // _STACK_H_


