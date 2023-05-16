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

#include "mm.h"
#include "memlib.h"

/*********************************************************
 * NOTE TO STUDENTS: Before you do anything else, please
 * provide your team information in the following struct.
 ********************************************************/
team_t team = {
    /* Team name */
    "KRAFTON JUNGLE",
    /* First member's full name */
    "Yong Hyun Kim",
    /* First member's email address */
    "yhkim8246@gmail.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

/* single word (4) or double word (8) alignment */
#define ALIGNMENT 8

/* rounds up to the nearest multiple of ALIGNMENT */
#define ALIGN(size) (((size) + (ALIGNMENT-1)) & ~0x7)
#define SIZE_T_SIZE (ALIGN(sizeof(size_t)))

/* Basic constants and macros */
#define WSIZE 4 /* Word 그리고 header/footer의 크기는 4byte */
#define DSIZE 8 /* Double word size (bytes) */
// #define TSIZE 12
// #define QSIZE 16
#define CHUNKSIZE (1<<12) /* Extend heap by this amount (bytes) */
#define MAX(x,y) ((x) > (y) ? (x) : (y))

/* Pack a size and allocated bit into a word */
#define PACK(size, alloc) ((size) | (alloc))

/* Read and write a word at address p */

#define GET(p)  (*(unsigned int *)(p))
#define PUT(p, val) (*(unsigned int *)(p) = (val))

/* Read the size and allocated fields from address p */
#define GET_SIZE(p) (GET(p) & ~0x7)
#define GET_ALLOC(p) (GET(p) & 0x1)

/* Given block ptr bp, compute address of its header and footer */
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)


/* Given block ptr bp, compute address of next and previous blocks */
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))


#define PRED(bp)  ((char *)(bp))
#define SUCC(bp)  ((char *)(bp) + WSIZE)

void add_list(char *bp);
void del_list(char *bp);
static char* coalesce(void *bp);
static void *extend_heap(size_t words);
int mm_init(void);
static void * find_fit(size_t asize);
static void place(void *bp, size_t asize);
void *mm_malloc(size_t size);
void mm_free(void *bp);
void *mm_realloc(void *ptr, size_t size);

static char * heap_listp;
static char * root;
/*
 * mm_init - initialize the malloc package.
 */
int mm_init(void)
{
    /* Create the initial empty heap */

    if ((heap_listp = mem_sbrk(6*WSIZE)) == (void *)-1) return -1;

    PUT(heap_listp, 0);                                /* Alignmnet padding */
    PUT(heap_listp + (1*WSIZE), PACK(2*DSIZE, 1));     /* Prologue header */
    PUT(heap_listp + (2*WSIZE), NULL);                 /* PRED */
    PUT(heap_listp + (3*WSIZE), NULL);                 /* SUCC */
    PUT(heap_listp + (4*WSIZE), PACK(2*DSIZE, 1));     /* Prologue footer */
    PUT(heap_listp + (5*WSIZE), PACK(0, 1));           /* Epilogue header */
    heap_listp+=(2*WSIZE);

    root = heap_listp;
    PUT(SUCC(root),PRED(root));
    PUT(PRED(root),root);
    

    /* Extend the empty heap with a free block of CHUNKSIZE bytes */
    if (extend_heap(CHUNKSIZE/WSIZE) == NULL) return -1;
    return 0;
}

 
/* 
 * mm_malloc - Allocate a block by incrementing the brk pointer.
 *     Always allocate a block whose size is a multiple of the alignment.
 */
void *mm_malloc(size_t size)
{
    size_t asize;      /* Adjusted block size */
    size_t extendsize; /* Amount to extend heap if no fit */
    char *bp;
    /* Ignore spurious requests */
    if (size == 0) return NULL;

    /* Adjust block size to include overhead and alignment reqs. */
    if (size <= DSIZE) asize = 2 * DSIZE;
    else asize = DSIZE * ((size + (DSIZE) + (DSIZE -1)) / DSIZE);

    /* Search the free list for a fit */
    if ((bp = find_fit(asize))!=NULL){
        place(bp,asize);
        return bp;
    }

    /* No fit found. Get more memory and place the block */
    extendsize = MAX(asize,CHUNKSIZE);
    if ((bp = extend_heap(extendsize/WSIZE))==NULL) return NULL;
    place(bp,asize);
    return bp;
}

static void *extend_heap(size_t words)
{
    char *bp;
    size_t size;

    /* Allocate an even number of words to maintain alignment */
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if((long)(bp = mem_sbrk(size))==-1) return NULL;
    /* Initialize free block header/footer and the epilogue header */
    PUT(HDRP(bp), PACK(size, 0));        /* Free block header */
    PUT(PRED(bp), NULL);                 /* PRED */
    PUT(SUCC(bp), NULL);                 /* SUCC */
    PUT(FTRP(bp), PACK(size, 0));        /* Free block footer */
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0,1)); /* New eiplogue header */
    
    /* Coalesce if the previous block was free */
    return coalesce(bp);
}

/*
 * mm_free - Freeing a block does nothing.
 */
void mm_free(void *bp)
{

    size_t size = GET_SIZE(HDRP(bp));

    PUT(HDRP(bp), PACK(size,0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}


void *mm_realloc(void *ptr, size_t size)
{
    size_t oldsize;
    void *newptr;

    /* If size == 0 then this is just free, and we return NULL. */
    if(size == 0) {
        mm_free(ptr);
        return 0;
    }

    /* If oldptr is NULL, then this is just malloc. */
    if(ptr == NULL) return mm_malloc(size);
    
    newptr = mm_malloc(size);

    /* If realloc() fails the original block is left untouched  */
    if(!newptr) return 0;

    /* Copy the old data. */
    oldsize = GET_SIZE(HDRP(ptr));
    if(size < oldsize) oldsize = size;
    memcpy(newptr, ptr, oldsize);

    /* Free the old block. */
    mm_free(ptr);

    return newptr;
}

static char* coalesce(void *bp)
{
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));
    
    if(prev_alloc && next_alloc){           /* Case 1 */
        add_list(bp);
    } 
    else if (prev_alloc && !next_alloc){        /* Case 2 */
        del_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
        add_list(bp);
    }
    else if(!prev_alloc && next_alloc){         /* Case 3 */
        size += GET(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp),PACK(size,0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size,0));
        bp = PREV_BLKP(bp);
    }
    else                                        /* Case 4 */
    {
        del_list(NEXT_BLKP(bp));
        size += GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        bp = PREV_BLKP(bp);

    }
    return bp;

}

static void * find_fit(size_t asize)
{
    /* Search from the rover to the end of list */
    for (char *cur = GET(SUCC(root)); !GET_ALLOC(HDRP(cur)); cur = GET(SUCC(cur)))
        if (asize <= GET_SIZE(HDRP(cur)))
            return cur;

    return NULL;
}

static void place(void *bp, size_t asize)
{
    size_t csize = GET_SIZE(HDRP(bp));   
    del_list(bp);
    if ((csize - asize) >= (2*DSIZE)) { 
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        coalesce(bp);
    }
    else { 
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


void add_list(char *bp){

    PUT( PRED(GET(SUCC(root))), bp);
    PUT( SUCC(bp), GET(SUCC(root)));
    PUT(SUCC(root), bp);
    PUT(PRED(bp), root);

}

void del_list(char *bp){ 
    PUT(SUCC(GET(PRED(bp))), GET(SUCC(bp)));
    PUT(PRED(GET(SUCC(bp))), GET(PRED(bp)));
}



