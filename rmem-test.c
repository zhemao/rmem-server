#include "rmem_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

int main(void)
{
    struct rmem_table rmem;
    char *data1, *data2, *data3, *data4;

    init_rmem_table(&rmem);

    data1 = rmem_alloc(&rmem, 100, 1);
    data2 = rmem_alloc(&rmem, 50, 2);
    data3 = rmem_alloc(&rmem, 120, 3);

    memset(data1, '1', 100);
    memset(data2, '2', 50);
    memset(data3, '3', 120);

    rmem_table_free(&rmem, data2);
    data2 = rmem_alloc(&rmem, 70, 2);
    memset(data2, '2', 70);

    data4 = rmem_alloc(&rmem, 25, 4);
    memset(data4, '4', 25);

    rmem_table_free(&rmem, data3);
    data3 = rmem_alloc(&rmem, 120, 3);
    memset(data3, '3', 120);

    assert(data1 == rmem_table_lookup(&rmem, 1));
    assert(data2 == rmem_table_lookup(&rmem, 2));
    assert(data3 == rmem_table_lookup(&rmem, 3));
    assert(data4 == rmem_table_lookup(&rmem, 4));

    dump_rmem_table(&rmem);
    free_rmem_table(&rmem);

    return 0;
}
