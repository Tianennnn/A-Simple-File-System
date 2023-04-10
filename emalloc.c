#include <stdio.h>
#include <stdlib.h>

/**
 * Function:  emalloc
 * --------------------
 * @brief calls malloc and handles the exception.
 *
 * @param size_t The size of the object to reserve dynamic memory for.
 *
 * @return: Void.
 *
 */

void *emalloc(size_t n) {
    void *p = NULL;
    p = malloc(n);
    if (p == NULL) {
        printf("Failed to malloc.\n");
        exit(-1);
    }
    return p;
}