#include "syscall.h"
#include "synchop.h"
int
main()
{
    int id1 = SemGet(800);
    int id2 = SemGet(500);
    int id3 = SemGet(800);

    PrintInt(id1);
    PrintChar('\n');
    PrintInt(id2);
    PrintInt(id3);
    int x = Fork();
    if(x==0)
    {
        int id4 = SemGet(500);
        PrintChar('\n');
        PrintInt(id4);
    }

   return 0;
}
