#include "syscall.h"

int
main()
{
    int x=0;
    //PrintInt(&x);
    int *array = (int*)ShmAllocate(2*sizeof(int));
//    Sleep(1);
    //int y=10;
    array[0] = 0;
    array[1] = 7;
    PrintInt(&array[1]);
    x = Fork();
    if (x == 0) {
	array[1]+=5;
	PrintInt(array[1]);
    }
    else {
       PrintInt(array[1]);
       Join(x);
    }
    return 0;
}
