#include "utils.h"

/* Between */
int betw(int x, int a, int b)
{
    return (x >= a && x <= b) ? TRUE : FALSE;
}

/* Between - exclusive */
int betwx(int x, int a, int b)
{
    return (x > a && x < b) ? TRUE : FALSE;
}