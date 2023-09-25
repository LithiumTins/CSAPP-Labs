#include <unistd.h>
#include <stdio.h>

int main(int argc, char argv[])
{
    getchar();
    getchar();
    printf("this is pause\n");
    while (1)
    {
        sleep(1);
        printf("hh\n");
    }
    printf("pause end\n");
    return 0;
}