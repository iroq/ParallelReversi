#include "utils.h"

/* Between */
int betw(int x, int a, int b)
{
    return (x >= a && x <= b) ? true : false;
}

/* Between - exclusive */
int betwx(int x, int a, int b)
{
    return (x > a && x < b) ? true : false;
}

int min(int a, int b)
{
    return (a<b)? a : b;
}

int max(int a, int b)
{
    return (a>b)? a : b;
}
