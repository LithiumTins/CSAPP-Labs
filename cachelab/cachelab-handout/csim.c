#define _POSIX_C_SOURCE 2
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "cachelab.h"

int hit_count = 0;
int miss_count = 0;
int eviction_count = 0;

struct node
{
    //char *block;
    long long sym;
    struct node *next;
};

struct node *cache;

int main(int argc, char *argv[])
{
    int opt, verbose = 0, s = 0, e = 0, b = 0, S;
    char *file_path = NULL;
    while ((opt = getopt(argc, argv, "hvs:E:b:t:")) != -1)
    {
        switch (opt)
        {
        case 'h':
            printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
            return 0;
        case 'v':
            verbose = 1;
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 'E':
            e = atoi(optarg);
            break;
        case 'b':
            b = atoi(optarg);
            break;
        case 't':
            file_path = optarg;
            break;
        default:
            printf("Unknown argument\n");
            return -1;
        }
    }

    if (s == 0 || b == 0 || e == 0 || file_path == NULL)
    {
        printf("Usage: ./csim-ref [-hv] -s <s> -E <E> -b <b> -t <tracefile>\n");
        return -1;
    }

    S = 1 << s;

    cache = calloc(S, sizeof(struct node));
    
    char ins[2];
    long long adr;
    int len;
    FILE *file = fopen(file_path, "r");
    if (file == NULL)
    {
        printf("Can't open file %s\n", file_path);
        return -1;
    }

    while (fscanf(file, "%s%llx,%d", ins, &adr, &len) != EOF)
    {
        long long symbol, set_code;//, block_ind;
        symbol = (adr >> (b + s)) & ((1 << (64 - b - s)) - 1);
        set_code = (adr >> b) & ((1 << s) - 1);

        if (ins[0] == 'I')
            continue;

        //printf("set_code is %d, symbol is %d\n", set_code, symbol);
        //block_ind = adr & ((1 << b) - 1);
        struct node *set_p = cache + set_code;
        struct node *p = set_p;
        while (p->next && p->next->sym != symbol)
            p = p->next;
        if (verbose)
            printf("%c %llx,%d ", ins[0], adr, len);
        if (p->next)
        {
            hit_count++;
            if (verbose)
                printf("hit");
            struct node *tmp = p->next;
            p->next = tmp->next;
            tmp->next = set_p->next;
            set_p->next = tmp;
        }
        else
        {
            miss_count++;
            if (verbose)
                printf("miss");
            if (set_p->sym < e)
            {
                set_p->sym++;
                struct node *tmp = malloc(sizeof(struct node));
                //tmp->block = calloc(1 << b, 1);
                tmp->sym = symbol;
                tmp->next = set_p->next;
                set_p->next = tmp;
            }
            else
            {
                if (verbose)
                    printf(" eviction");
                p->sym = symbol;
                eviction_count++;
                struct node *tmp = p;
                p = set_p;
                while (p->next != tmp)
                    p = p->next;
                p->next = tmp->next;
                tmp->next = set_p->next;
                set_p->next = tmp;
            }
        }
        if (ins[0] == 'M')
        {
            if (verbose)
                printf(" hit");
            hit_count++;
        }
        if (verbose)
            printf("\n");
    }

    printSummary(hit_count, miss_count, eviction_count);
    return 0;
}
