#include "syscall.h"
#define OUTER_BOUND 4
#define SIZE 100

int
main()
{
    int array[SIZE], i, k, sum, pid=GetPID()-1;
    unsigned start_time, end_time;
    
    start_time = GetTime();
    for (k=0; k<OUTER_BOUND; k++) {
       for (i=0; i<SIZE; i++) sum += array[i];
       PrintInt(pid);
       PrintInt(pid);
    }
    end_time = GetTime();
    PrintChar('\n');
    PrintString("Total sum: ");
    PrintInt(sum);
    PrintChar('\n');
    PrintString("Start time: ");
    PrintInt(start_time);
    PrintString(", End time: ");
    PrintInt(end_time);
    PrintString(", Total time: ");
    PrintInt(end_time-start_time);
    PrintChar('\n');
    return 0;
}
