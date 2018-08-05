/**
Quick allocation and freeing of unique 32 bit ID numbers.
*/

#ifndef OBJID_C
#define OBJID_C

#define OBJS_PER_BLOCK ((1024*16)-(sizeof(void*)/sizeof(ID)))
#define MAX_OBJS (1<<29)
#include <stdint.h>

typedef uint32_t ID;

ID objid_new(void);
void objid_free(ID obj);
void objid_stats(void);

#if __INCLUDE_LEVEL__ == 0

#include "logging.c"

typedef struct s_FreeBlock FreeBlock;

struct s_FreeBlock {
	FreeBlock *next;
	ID objs[OBJS_PER_BLOCK];
};

struct {
	int nobjs;
	int max_id;
	FreeBlock *root;
	FreeBlock *spare;
} g_free = {0};

void objid_stats(void)
{
	int nblks = 0;
	FreeBlock *blk = g_free.root;
	while (blk && ++nblks)
		blk = blk->next;
	
	INFO("ID: [%d objs] [%d / block] [%d+%d freed] [%d blocks] [Spare?%d]",
		g_free.max_id,
		OBJS_PER_BLOCK,
		g_free.nobjs,
		nblks?(nblks-1)*OBJS_PER_BLOCK : 0,
		nblks,
		!!g_free.spare);
}

ID objid_new(void)
{
	if (g_free.nobjs == 0) {
		if (g_free.max_id == MAX_OBJS)
			ABORT("Out of objects ");
		return ++g_free.max_id;
	}
	if (g_free.nobjs > 1)
		return g_free.root->objs[--g_free.nobjs];
	
	if (g_free.spare)
		free(g_free.spare);
	g_free.spare = g_free.root;
	g_free.root = g_free.spare->next;
	g_free.nobjs = g_free.root? OBJS_PER_BLOCK : 0;
	return g_free.spare->objs[0];
}

void objid_free(ID obj)
{
	if (obj == g_free.max_id) {
		--g_free.max_id;
		return;
	}
	if (!g_free.root || g_free.nobjs == OBJS_PER_BLOCK) {
		if (!g_free.spare)
			g_free.spare = (FreeBlock*)malloc(sizeof(FreeBlock));
		g_free.nobjs = 0;
		g_free.spare->next = g_free.root;
		g_free.root = g_free.spare;
		g_free.spare = NULL;
	}
	
	g_free.root->objs[g_free.nobjs++] = obj;
}


#endif
#endif
