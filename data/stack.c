#include <stdlib.h>
#include <assert.h>
#include <string.h>

#include "stack.h"
#include "list.h"

struct stack 
{
    list_t stack_list;
};

/*
 * PUBLIC METHODS
 */

stack_p stack_create() 
{
    stack_p stack = 0;
    stack = (stack_p)malloc(sizeof(struct stack));
    assert(stack);

    stack->stack_list = list_create();

    return stack;
}

void stack_push(stack_p stack, void* data) 
{
    list_push_back(stack->stack_list, data);
}


void stack_destroy(stack_p* stack) 
{
    list_destroy(&(*stack)->stack_list);
    free(*stack);
    *stack = 0;
}

void* stack_pop(stack_p stack) 
{
    assert(list_size(stack->stack_list));
    return list_pop_back(stack->stack_list);
}

int stack_size(stack_p stack) 
{
    return list_size(stack->stack_list);
}

