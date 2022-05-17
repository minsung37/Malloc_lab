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
    "-",
    /* First member's full name */
    "-",
    /* First member's email address */
    "-",
    /* Second member's full name (leave blank if none) */
    "",
    /* Second member's email address (leave blank if none) */
    "",

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

// Free List 상에서의 이전, 이후 블록의 포인터를 리턴한다.
#define PREC_FREEP(bp)  (*(void**)(bp))             // 이전 블록의 bp
#define SUCC_FREEP(bp)  (*(void**)(bp + WSIZE))     // 이후 블록의 bp


// free list의 맨 첫 블록을 가리키는 포인터
static char* free_listp = NULL;


// 새로 반환되거나 생성된 가용 블록을 free list의 첫 부분에 넣는다.
void putFreeBlock(void* bp){
    // 이후 블록의 bp에 들어있는 주소값을 리턴
    // bp가 포함된 블럭의 successor는 free_listp이다
    SUCC_FREEP(bp) = free_listp;

    // 이전 블록의 bp에 들어있는 주소값을 리턴
    // bp가 포함된 블럭의 predecessor는 NULL이다.     
    PREC_FREEP(bp) = NULL;

    // bp의 주소값은 free_listp 이전블록에 들어있는 주소값
    PREC_FREEP(free_listp) = bp;
    free_listp = bp;
}


// 할당되거나 연결되는 가용 블록을 free list에서 없앤다.
void removeBlock(void *bp)
{
    // free list의 첫번째 블록을 없앨 때
    if (bp == free_listp){
        PREC_FREEP(SUCC_FREEP(bp)) = NULL;
        free_listp = SUCC_FREEP(bp);
    }
    // free list 안에서 없앨 때
    else{
        SUCC_FREEP(PREC_FREEP(bp)) = SUCC_FREEP(bp);
        PREC_FREEP(SUCC_FREEP(bp)) = PREC_FREEP(bp);
    }
}


// 앞뒤 블록 확인해 합치고 연결
static void* coalesce(void* bp)
{
    // 1 == 할당 == True, 0 == 가용 == False
    size_t prev_alloc = GET_ALLOC(FTRP(PREV_BLKP(bp)));  // 직전 블록 가용 여부
    size_t next_alloc = GET_ALLOC(HDRP(NEXT_BLKP(bp)));  // 직후 블록 가용 여부
    size_t size = GET_SIZE(HDRP(bp));
    // init => extend 에서 호출될때는 prev_alloc = 1, next_alloc = 1
    // case 1 : 직전, 직후 블록이 모두 할당 -> 해당 블록만 free list에 넣어주면 된다.

    // case 2: 앞쪽은 alloc 되어 있고 뒤쪽은 free
    if(prev_alloc && !next_alloc){
        removeBlock(NEXT_BLKP(bp));
        size = size + GET_SIZE(HDRP(NEXT_BLKP(bp)));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));
    }
    // case 3: 앞쪽은 free 되어 있고 뒤쪽은 alloc
    else if(!prev_alloc && next_alloc){
        bp = PREV_BLKP(bp); 
        removeBlock(bp);
        size = size + GET_SIZE(HDRP(bp));
        PUT(HDRP(bp), PACK(size, 0));
        PUT(FTRP(bp), PACK(size, 0));  
    }
    // case 4: 앞뒤 둘다 free
    else if (!prev_alloc && !next_alloc) {
        removeBlock(PREV_BLKP(bp));
        removeBlock(NEXT_BLKP(bp));
        size = size + GET_SIZE(HDRP(PREV_BLKP(bp))) + GET_SIZE(FTRP(NEXT_BLKP(bp)));
        SUCC_FREEP(NEXT_BLKP(bp)) = NULL;
        bp = PREV_BLKP(bp);
        PUT(HDRP(bp), PACK(size, 0));  
        PUT(FTRP(bp), PACK(size, 0));
    } 
    // 연결된 새 가용 블록을 free list에 추가한다.
    putFreeBlock(bp);
    return bp;
}


// 새 가용 블록으로 힙 확장하기
static void* extend_heap(size_t words)
{
    char* bp;
    size_t size;

    size = (words % 2) ? (words + 1) * WSIZE : (words) * WSIZE;
    // 이 때의 bp값은 확장하기전의 가장 끝위치
    if ((long)(bp = mem_sbrk(size)) == -1)
        return NULL;

    // 새 가용 블록의 header와 footer를 정해주고 epilogue block을 가용 블록 맨 끝으로 옮긴다.
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    PUT(HDRP(NEXT_BLKP(bp)), PACK(0, 1));

    // 만약 이전 블록이 가용 블록이라면 통합
    return coalesce(bp);
}


/*  mm_init - initialize the malloc package. */
int mm_init(void)
{
    char* heap_listp;
    // 메모리에서 6words를 가져오고 이걸로 빈 가용 리스트 초기화
    if ((heap_listp = mem_sbrk(6 * WSIZE)) == (void*)-1)
        return -1;

    // 초기할당
    PUT(heap_listp, 0);  // Alignment padding 더블 워드 경계로 정렬된 미사용 패딩.
    PUT(heap_listp + (1 * WSIZE), PACK(2 * DSIZE, 1));
    PUT(heap_listp + (2 * WSIZE), NULL);
    PUT(heap_listp + (3 * WSIZE), NULL);
    PUT(heap_listp + (4 * WSIZE), PACK(2 * DSIZE, 1));
    PUT(heap_listp + (5 * WSIZE), PACK(0, 1));

    // free_listp를 탐색의 시작점
    free_listp = heap_listp + 2 * WSIZE;

    // 6 * WSIZE 만큼 힙을 확장해 초기 가용 블록을 생성
    if (extend_heap(6) == NULL)
        return -1;
    return 0;
}


/* mm_free - Freeing a block does nothing. */
void mm_free(void *bp)
{
    size_t size = GET_SIZE(HDRP(bp));
    PUT(HDRP(bp), PACK(size, 0));
    PUT(FTRP(bp), PACK(size, 0));
    coalesce(bp);
}


static void* find_fit(size_t asize)
{
    void* bp;
    // Free list의 맨 뒤는 Free list에서 유일하게 할당된 블록
    for (bp = free_listp; GET_ALLOC(HDRP(bp)) != 1; bp = SUCC_FREEP(bp)){
        if(asize <= GET_SIZE(HDRP(bp))){
            return bp;
        }
    }
    return NULL;
}


static void place(void* bp, size_t asize)
{
    // 현재 할당할 수 있는 후보 가용 블록의 주소
    size_t csize = GET_SIZE(HDRP(bp));

    // 할당될 블록이므로 free list에서 없애준다.
    removeBlock(bp);

    // 분할이 가능한 경우
    if ((csize - asize) >= (2 * DSIZE)){
        // 앞의 블록은 할당 블록으로
        PUT(HDRP(bp), PACK(asize, 1));
        PUT(FTRP(bp), PACK(asize, 1));
        // 뒤의 블록은 가용 블록으로 분할한다.
        bp = NEXT_BLKP(bp);
        PUT(HDRP(bp), PACK(csize-asize, 0));
        PUT(FTRP(bp), PACK(csize-asize, 0));
        // free list 첫번째에 분할된 블럭을 넣는다.
        putFreeBlock(bp);
    }
    else{
        PUT(HDRP(bp), PACK(csize, 1));
        PUT(FTRP(bp), PACK(csize, 1));
    }
}


/*  mm_malloc - Allocate a block by incrementing the brk pointer.
    Always allocate a block whose size is a multiple of the alignment. */
void *mm_malloc(size_t size)
{
    size_t adjusted_size;       // Adjusted block size
    size_t extendsize;  // Amount for extend heap if there is no fit
    char* bp;
    // 할당받은 요청이 0이면 NULL 반환
    if (size == 0)
        return NULL;
    // 2words 이하 사이즈면 4 워드로 할당 요청해라 (header에 1word, footer 1word)
    if (size <= DSIZE) {
        adjusted_size = 2 * DSIZE;
    }
    // (요청된 용량 + 헤더의 크기) 보다 큰 8의 배수를 찾는다
    else {
        adjusted_size = DSIZE * ((size + (DSIZE) + (DSIZE - 1)) / DSIZE);
    }
    // 적당한 크기의 가용 블록을 검색
    if ((bp = find_fit(adjusted_size)) != NULL){
        // place함수로 초과 부분을 분할하고 새롭게 할당한 블록의 블록 포인터를 반환
        place(bp, adjusted_size);
        return bp;
    }

    // 적당한 가용 블록을 찾지 못한다면 extend_heap함수로 heap을 확장하여 추가 확장 블록을 배정
    extendsize = MAX(adjusted_size, CHUNKSIZE);
    if ((bp = extend_heap(extendsize / WSIZE)) == NULL) 
        return NULL;
    place(bp, adjusted_size);
    return bp;
}


/* mm_realloc - Implemented simply in terms of mm_malloc and mm_free */
void *mm_realloc(void *ptr, size_t size)
{
    // 크기를 조절하고 싶은 힙의 시작 포인터
    void *oldptr = ptr;
    // 크기 조절 뒤의 새 힙의 시작 포인터
    void *newptr;
    // 복사할 힙의 크기
    size_t copySize;
    
    newptr = mm_malloc(size);
    if (newptr == NULL)
      return NULL;

    copySize = GET_SIZE(HDRP(oldptr));
    // 원래 메모리 크기보다 적은 크기를 realloc하면 크기에 맞는 메모리만 할당되고 나머지는 안 된다. 
    if (size < copySize)
      copySize = size;

    // newptr에 oldptr를 시작으로 copySize만큼의 메모리 값을 복사한다.
    memcpy(newptr, oldptr, copySize);
    // 기존의 힙을 반환한다.
    mm_free(oldptr);
    return newptr;
}