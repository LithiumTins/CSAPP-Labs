/*
 * mm-naive.c - The fastest, least memory-efficient malloc package.
 *
 * In this naive approach, a block is allocated by simply incrementing
 * the brk pointer.  A block is pure payload. There are no headers or
 * footers.  Blocks are never coalesced or reused. Realloc is
 * implemented directly using mm_malloc and mm_free.
 *
 * NOTE TO STUDENTS: Replace this header comment with your own header
 * comment that gives a high level description of your solution.
 */
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "ateam",
    /* First member's full name */
    "Harry Bovik",
    /* First member's email address */
    "bovik@cs.cmu.edu",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT - 1)) & ~0x7)

#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

#define INIT_SIZE 1280
#define MAX_HEAP (20 * (1 << 20))

static int cur_size;
static char *my_mem;
static char *header;
static char *footer;

#define WSIZE 4
#define DSIZE 8
#define IN_USED 1
#define NO_USED 0

#define PACK(size, used) ((size) | (used))

#define GET(p) (*(unsigned int *)(p))
#define PUT(p, data) (*(unsigned int *)(p) = (data))

#define SIZE(p) (GET(p) & ~0x7)
#define USED(p) (GET(p) & 0x1)

#define HDR(p) ((char *)(p) - WSIZE)
#define FTR(p) ((char *)(p) + SIZE(HDR(p)) - DSIZE)

#define GET_SIZE(p) (SIZE(HDR(p)))
#define GET_USED(p) (USED(HDR(p)))

#define NEXTP(p) ((char *)(p) + GET_SIZE(p))
#define PREVP(p) ((char *)(p) - SIZE(((char *)(p) - DSIZE)))

#define REACH_END(p) (GET_SIZE(p) == 0)

/*
 * best_fit() - search for an empty block with best-fit
 */
char *best_fit(int size)
{
    char *p = header, *best = NULL;
    
    int psize, min_size = MAX_HEAP;
    while ((psize = GET_SIZE(p)) != 0)
    {
        if (GET_USED(p) == NO_USED && psize >= size && psize < min_size)
        {
            best = p;
            min_size = psize;
        }
        p = NEXTP(p);
    }
    return best;
}

/*
 * next_fit() - search an empty block with next-fit
 */
static char *start_p;
char *next_fit(int size)
{
    char *p = start_p;
    int psize;
    do
    {
        psize = GET_SIZE(p);
        if (GET_USED(p) == NO_USED && psize >= size)
        {
            return p;
        }
        p = psize ? NEXTP(p) : header;
    } while (p != start_p);

    return NULL;
}


/*
 * first_fit() - search an empty block with first-fit
 */
char *first_fit(int size)
{
    char *p = start_p;
    int psize;
    while ((psize = GET_SIZE(p)) != 0)
    {
        if (GET_USED(p) == NO_USED && psize >= size)
        {
            return p;
        }
        p = NEXTP(p);
    }
    return NULL;
}

/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    // Allocate all the memory will be needed(MAX_HEAP)
    my_mem = mem_sbrk(INIT_SIZE);
    if (!my_mem)
        return -1;
    header = my_mem + DSIZE;
    footer = my_mem + INIT_SIZE;
    cur_size = INIT_SIZE;

    // Put a guardian header and footer
    PUT(HDR(header), PACK(8, IN_USED));
    PUT(FTR(header), PACK(8, IN_USED));
    PUT(HDR(footer), PACK(0, IN_USED));

    // Set the initial empty block
    char *p = NEXTP(header);
    int size = INIT_SIZE - 2 * DSIZE;
    PUT(HDR(p), PACK(size, NO_USED));
    PUT(FTR(p), PACK(size, NO_USED));

    // Initialize next fit
    start_p = header;
    return 0;
}

/*
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    if (size == 0)
        return NULL;

    // int newsize = ALIGN(size + SIZE_T_SIZE);
    int newsize = ALIGN(size + DSIZE);
    
    // search for an empty block bigger than size
    char *new_p = best_fit(newsize);
    if (!new_p)
    {
        char *last_p = PREVP(footer);

        if (GET_USED(last_p) == NO_USED)
        {
            // get the rest we need
            int lastsize = GET_SIZE(last_p), alloc_size = newsize - lastsize;
            char *new_brk = mem_sbrk(alloc_size);
            if (!new_brk)
                return NULL;
            // set new footer
            footer += alloc_size;
            PUT(HDR(footer), PACK(0, IN_USED));
            // set the new block
            PUT(HDR(last_p), PACK(newsize, IN_USED));
            PUT(FTR(last_p), PACK(newsize, IN_USED));
            cur_size += alloc_size;
            new_p = last_p;
        }
        else
        {
            // directory alloc a memory block
            new_p = mem_sbrk(newsize);
            if (!new_p)
                return NULL;
            // set new footer
            footer += newsize;
            PUT(HDR(footer), PACK(0, IN_USED));
            // set the new block
            PUT(HDR(new_p), PACK(newsize, IN_USED));
            PUT(FTR(new_p), PACK(newsize, IN_USED));
            cur_size += newsize;
        }
    }
    
    int psize = GET_SIZE(new_p);
    
    // put the metadata of the new block
    PUT(HDR(new_p), PACK(newsize, IN_USED));
    PUT(FTR(new_p), PACK(newsize, IN_USED));

    // if there is a remaining block, put according metadata
    int remsize = psize - newsize;
    if (remsize)
    {
        char *rem_p = NEXTP(new_p);
        PUT(HDR(rem_p), PACK(remsize, NO_USED));
        PUT(FTR(rem_p), PACK(remsize, NO_USED));
    }
    
    return new_p;
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *ptr)
{
    int size = GET_SIZE(ptr);
    char *pre = PREVP(ptr), *nxt = NEXTP(ptr);

    // try to combine the block before ptr
    if (GET_USED(pre) == NO_USED)
    {
        ptr = pre;
        size += GET_SIZE(pre);
    }

    // try to combine the block after ptr
    if (GET_USED(nxt) == NO_USED)
    {
        size += GET_SIZE(nxt);
    }

    // set the metadata
    PUT(HDR(ptr), PACK(size, NO_USED));
    PUT(FTR(ptr), PACK(size, NO_USED));
}
 
/*
 * mm_realloc - Implemented simply in terms of mm_malloc and mm_free
 */
void *mm_realloc(void *ptr, size_t size)
{
    if (size == 0)
    {
        mm_free(ptr);
        return NULL;
    }
    if (ptr == NULL)
    {
        return mm_malloc(size);
    }

    int newsize = ALIGN(size + DSIZE), oldsize = GET_SIZE(ptr);

    // do nothing when new_size == oldsize
    if (newsize == oldsize)
    {
        return ptr;
    }
    // devide the block into two parts when ask for a smaller block
    if (newsize < oldsize)
    {
        int remsize = oldsize - newsize;
        if (GET_USED(NEXTP(ptr)) == NO_USED)
            remsize += GET_SIZE(NEXTP(ptr));
        // change the size of block ptr
        PUT(HDR(ptr), PACK(newsize, IN_USED));
        PUT(FTR(ptr), PACK(newsize, IN_USED));
        // free the remaining block
        char *rem_p = NEXTP(ptr);
        PUT(HDR(rem_p), PACK(remsize, NO_USED));
        PUT(FTR(rem_p), PACK(remsize, NO_USED));
        return ptr;
    }
    // a bit more complex when ask for a bigger block
    if (newsize > oldsize)
    {
        // combine near blocks to see if big enough
        int comsize = oldsize;
        // try to only use next block
        char *nxt = NEXTP(ptr);
        if (GET_USED(nxt) == NO_USED)
        {
            comsize += GET_SIZE(nxt);
            if (comsize >= newsize)
            {
                PUT(HDR(ptr), PACK(newsize, IN_USED));
                PUT(FTR(ptr), PACK(newsize, IN_USED));
                int remsize = comsize - newsize;
                char *rem_p = NEXTP(ptr);
                if (remsize)
                {
                    PUT(HDR(rem_p), PACK(remsize, NO_USED));
                    PUT(FTR(rem_p), PACK(remsize, NO_USED));
                    return ptr;
                }
            }
        }
        // try to also use previous block
        char *pre = PREVP(ptr);
        if (GET_USED(pre) == NO_USED)
        {
            comsize += GET_SIZE(pre);
            if (comsize >= newsize)
            {
                memmove(pre, ptr, oldsize - DSIZE);
                PUT(HDR(pre), PACK(newsize, IN_USED));
                PUT(FTR(pre), PACK(newsize, IN_USED));
                int remsize = comsize - newsize;
                char *rem_p = NEXTP(pre);
                PUT(HDR(rem_p), PACK(remsize, NO_USED));
                PUT(FTR(rem_p), PACK(remsize, NO_USED));
                return pre;
            }
        }
        // free ptr first
        mm_free(ptr);
        // get a block big enough and copy old contents
        char *new_p = mm_malloc(newsize);
        memmove(new_p, ptr, oldsize - DSIZE);
        return new_p;
    }

    return NULL;
}