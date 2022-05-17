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
    "7",
    /* First member's full name */
    "minsung",
    /* First member's email address */
    "jiminsung@naver.com",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    ""
};

// 기본상수와 매크로
#define WSIZE 4 // 워드크기(한칸)
#define DSIZE 8 // 더블워드(두칸)
#define CHUNKSIZE (1<<12) // 4096bytes => 4kb, 새로 할당받는 힙의 크기
#define MAX(x, y) ((x) > (y) ? (x) : (y))

// PACK : 크기와 할당 비트를 통합해서 header와 footer에 저장할 수 있는 값 리턴(size 1~7, alloc 1or0)
#define PACK(size, alloc) ((size) | (alloc))
// GET : 인자 p가 참조하는 워드를 읽어서 리턴
#define GET(p)  (*(unsigned int *)(p))
// PUT : 인자 p가 가리키는 워드에 val을 저장
#define PUT(p, val)  (*(unsigned int *)(p) = (val))

// 각각 주소 p에 있는 header 또는 footer의 size와 할당비트를 리턴한다. 
#define GET_SIZE(p)  (GET(p) & ~0x7)
#define GET_ALLOC(p)  (GET(p) & 0x1)

// bp(현재 블록 포인터)로 현재 블록의 header 위치와 footer 위치 반환
#define HDRP(bp) ((char *)(bp) - WSIZE)
#define FTRP(bp) ((char *)(bp) + GET_SIZE(HDRP(bp)) - DSIZE)

// 다음과 이전 블록 포인터 반환
#define NEXT_BLKP(bp) ((char *)(bp) + GET_SIZE(((char *)(bp) - WSIZE)))
#define PREV_BLKP(bp) ((char *)(bp) - GET_SIZE(((char *)(bp) - DSIZE)))

// 힙의 현재위치를 나타내는 포인터
void *heap_listp;


// 앞뒤 블록 확인해 합치기
static void *coalesce(void *bp)
{
    // 1 == 할당 == True, 0 == 가용 == False
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));
    size_t size = GET_SIZE(HDRP(bp));

    // case 1: 현재 블록을 기준으로 앞뒤 블록 다 alloc되어 있다면
    if (prev_alloc && next_alloc) { 
        return bp;
    }

    // case 2: 앞쪽은 alloc 되어 있고 뒤쪽은 free
    else if (prev_alloc && !next_alloc) { 
        // 뒤쪽 블럭의 사이즈를 더함
        size = size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }

    // case 3: 앞쪽은 free 되어 있고 뒤쪽은 alloc
    else if (!prev_alloc && next_alloc) {
        // 앞쪽 블럭의 사이즈를 더함
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp)));
        PUT(FTRP(bp), PACK(size, 0));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        // 블럭포인트를 앞으로 옮긴다
        bp = PREV_BLKP(bp);
    }

    // case 4: 앞뒤 둘다 free
    else {
        // 앞쪽, 뒤쪽 블럭의 사이즈를 더함
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        PUT(HDRP(PREV_BLKP(bp)), PACK(size, 0));
        PUT(FTRP(NEXT_BLKP(bp)), PACK(size, 0));
        // 블럭포인트를 앞으로 옮긴다
        bp = PREV_BLKP(bp);
    }
    return bp;
}


// 새 가용 블록으로 힙 확장하기
static void *extend_heap(size_t words) {
    char *bp;
    size_t size;

    // words 짝수로 공간할당
    size = (words % 2) ? (words+1) * WSIZE : words * WSIZE;
    if ((long)(bp = mem_sbrk(size)) == -1) {
        return NULL;
    }

    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 새로 확장한 블록의 전이 가용 블록이라면 통합
    return coalesce(bp);
}


/*  mm_init - initialize the malloc package.*/
int mm_init(void)
{
    /* Create the initial empty heap */
    // heap_listp가 힙의 최댓값 이상을 요청하면 fail
    if ((heap_listp = mem_sbrk(4 * WSIZE)) == (void *)-1) {
        return -1;
    }

    // 초기할당을 위해 4워드의 가용리스트를 만든다.
    PUT(heap_listp, 0);                               /* Alignment padding */
    PUT(heap_listp + (1 * WSIZE), PACK(DSIZE, 1));    /* Prologue header */
    PUT(heap_listp + (2 * WSIZE), PACK(DSIZE, 1));    /* Prologue footer*/
    PUT(heap_listp + (3 * WSIZE), PACK(0, 1));        /* Epilogue header */

    // 현재위치를 나타내는 포인터
    heap_listp = heap_listp + (2 * WSIZE);

    // 할당가능한 힙을 초과한 경우 NULL 반환 
    if (extend_heap(CHUNKSIZE / WSIZE) == NULL) {
        return -1;
    }
    return 0;
}


/* mm_free - Freeing a block does nothing. */
void mm_free(void *bp)
{
    // 반환 하려는 블록의 header와 footer로 가서 0입력
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}


// First-fit
static void *find_fit(size_t adjusted_size) {
    void *bp;
    // 다음블럭의 헤더로 이동하면서 진행
    for (bp = heap_listp; GET_SIZE(HDRP(bp)) > 0; bp = NEXT_BLKP(bp)) {
        // 가용상태이고 원하는 블럭보다 크면
        if (!GET_ALLOC(HDRP(bp)) && (adjusted_size <= GET_SIZE(HDRP(bp)))) {
            return bp;   
        }
    }
    return NULL;
}


static void place(void *bp, size_t adjusted_size) {
    size_t current_size = GET_SIZE(HDRP(bp));

    if ((current_size - adjusted_size) >= (2 * (DSIZE))) {
        // 요청 용량만큼 블록 배치
        PUT(HDRP(bp), PACK(adjusted_size, 1));
        PUT(FTRP(bp), PACK(adjusted_size, 1));
        // 다음 블럭으로 블럭포인터 이동
        bp = NEXT_BLKP(bp);
        // 남은 블록에 header, footer 배치
        PUT(HDRP(bp), PACK(current_size - adjusted_size, 0));
        PUT(FTRP(bp), PACK(current_size - adjusted_size, 0));
    }

    else {
        // current_size와 adjusted_size 차이가 네 칸(16바이트) 보다 작다면 해당 블록을 통째로 사용한다
        PUT(HDRP(bp), PACK(current_size, 1));
        PUT(FTRP(bp), PACK(current_size, 1));
    }
}


/* mm_malloc - Allocate a block by incrementing the brk pointer.
   Always allocate a block whose size is a multiple of the alignment. */
void *mm_malloc(size_t size)
{
    size_t adjusted_size;
    size_t extend_size;
    char *bp;
    // 할당받은 요청이 0이면 NULL 반환
    if (size == 0) {
        return NULL;
    }
    // 2words 이하 사이즈면 4 워드로 할당 요청해라 (header에 1word, footer 1word)
    if (size <= DSIZE) {
        adjusted_size = 2 * DSIZE;
    }
    // (요청된 용량 + 헤더의 크기) 보다 큰 8의 배수를 찾는다
    else {
        adjusted_size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    // 적당한 크기의 가용 블록을 검색
    if ((bp = find_fit(adjusted_size)) != NULL) {
        // place함수로 초과 부분을 분할하고 새롭게 할당한 블록의 블록 포인터를 반환
        place(bp, adjusted_size);
        return bp;
    }
    // 적당한 가용 블록을 찾지 못한다면 extend_heap함수로 heap을 확장하여 추가 확장 블록을 배정
    extend_size = MAX(adjusted_size, CHUNKSIZE);
    if ((bp = extend_heap(extend_size / WSIZE)) == NULL) {
        return NULL;
    }
    place(bp, adjusted_size);
    return bp;
}


/* mm_realloc - Implemented simply in terms of mm_malloc and mm_free */
void *mm_realloc(void *ptr, size_t size)
{
    void *oldptr = ptr;
    void *newptr;
    size_t copySize;

    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;
    
    copySize = GET_SIZE(HDRP(oldptr));
    if (size < copySize)
      copySize = size;
    
    memcpy(newptr, oldptr, copySize);
    mm_free(oldptr);
    return newptr;
}