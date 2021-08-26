#ifndef STRCOMMON_H
#define STRCOMMON_H

#include <string.h>

/**
 * Reverses a string array */
void reverse(char *c)
{
    int n = strlen(c);
    int i, j;
    char tmp;
    for (i = 0, j = n - 1; i < j; i++, j--)
    {
        // swap characters from ends of the string and move towards centre.
        tmp = c[i];
        c[i] = c[j];
        c[j] = tmp;
    }
}

#endif