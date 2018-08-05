#ifndef MMAP_LIST_C
#define MMAP_LIST_C

#include <stdint.h>

#define OBJ_SIZE 64
#define PAGE_SIZE 4096
#define OBJ_PER_PAGE  (PAGE_SIZE/OBJ_SIZE)
#define N_SUBPAGES (OBJ_SIZE/4 - 2)
#define MAX_PAGES (1<<17)

typedef uint32_t ObjRef;

typedef struct s_MMObj MMObj;

struct s_MMObj {
	ObjRef next;
	uint32_t depth; // the number of nodes under this one
	ObjRef sub[N_SUBPAGES];
};

extern MMObj O[]; // 

void objm_init(void);
ObjRef objm_alloc(void);
void objm_free(ObjRef obj);

void objm_debug(const char *filename);



#if __INCLUDE_LEVEL__ == 0

#include <unistd.h>
#include <errno.h>
#include <error.h>
#include <stdio.h>
#include <string.h>
#include <sys/mman.h>
#include "logging.c"


MMObj O[MAX_PAGES];
ObjRef objm_root;
int npages;


static void page_new(void)
{
	if (npages == MAX_PAGES)
		ABORT("Out of memory");
	
	ObjRef pg = npages * OBJ_PER_PAGE;
	
	ObjRef i;
	for (i = 1; i < OBJ_PER_PAGE-1; ++i)
		O[pg + i].next = pg+i+1;
	O[pg + i].next = 0;
	
	memset(&O[pg], 0, OBJ_SIZE);
	O[pg].next = pg + 1;

	objm_root = pg;
	INFO("New Page %u  %d", pg, O[objm_root].next);
	++npages;
	
}

uint32_t page_push(ObjRef a, ObjRef *p_b)
{
	ObjRef x, b = *p_b;
	uint32_t tmp;
	if (a < b) {
		tmp = O[a].next;
		O[a] = O[b];
		O[a].next = tmp;
		*p_b = a;
		a = b;
		b = *p_b;
	}			
	// we are now assured that a > b
	
	int i, best_i = -1, xdepth, best_depth = 1000000;
	for (i = 0; i < N_SUBPAGES; ++i) {
		x = O[b].sub[i];
		if (x == 0)
			break;
		xdepth = O[x].depth;
		if (xdepth < best_depth || xdepth == best_depth && a > x) {
			best_depth = xdepth;
			best_i = i;
		}
	}
	
	if (i == N_SUBPAGES) { // all the spots are full
		uint32_t depth = page_push(a, &O[b].sub[best_i]) + 1;
		if (depth > O[b].depth)
			O[b].depth = depth;
		return O[b].depth;
		
	} else { // i points to a zero space. add as a leaf
		O[b].sub[i] = a;
		tmp = O[a].next;
		memset(&O[a], 0, sizeof(MMObj));
		O[a].next = tmp;
		if (O[b].depth == 0)
			O[b].depth = 1;
		return 1;
	}
}

void page_depth_recalc(ObjRef p)
{
	int depth = -1;
	for (int i = 0; i < N_SUBPAGES; ++i) {
		ObjRef x = O[p].sub[i];
		if (x && (int)O[x].depth > depth)		
			depth = O[x].depth;
	}
	O[p].depth = depth + 1;
}

// remove a from *b
ObjRef page_pop(ObjRef b)
{
	ObjRef x, a = ~0;
	int i, i_best = -1;
	// find the smallest in b
	for (i = 0; i < N_SUBPAGES; ++i) {
		x = O[b].sub[i];
		if (x && x < a) {
			a = x;
			i_best = i;
		}
	}
	
	if (i_best < 0) {
		return 0;
		
	} else {
		O[b].sub[i_best] = page_pop(a);
		uint32_t tmp = O[a].next;
		O[a] = O[b];
		O[a].next = tmp;
		page_depth_recalc(a);
		
		return a;
	}
}

void objm_init(void)
{
	//~ O = (MMObj*)mmap(O, PAGE_SIZE * MAX_PAGES,  PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
	//~ if (O == MAP_FAILED) {
		//~ perror("mmap");
		//~ ABORT("mmap");
	//~ }
	npages = 0;
	page_new();
	objm_alloc(); // allocate one so that we never empty all pages
	//~ INFO("O = %p", O);
	//~ INFO("MMObj = %d", sizeof(MMObj));
	//~ INFO("N_SUBPAGES = %d", N_SUBPAGES);
	//~ INFO("OBJ_PER_PAGE = %d", OBJ_PER_PAGE);
}

ObjRef objm_alloc(void)
{ 
	//~ INFO("Alloc from %d : %d", objm_root,  O[objm_root].next);
	ObjRef out = O[objm_root].next;
	O[objm_root].next = O[out].next;
	if (!O[objm_root].next) {
		ObjRef x = page_pop(objm_root);
		if (x == 0) {
			page_new();
		} else {
			objm_root = x;
		}
	}
	return out;
}

void objm_free(ObjRef obj)
{
	ObjRef pg = (obj / OBJ_PER_PAGE) * OBJ_PER_PAGE;
	//~ INFO("Free: %u,  from %d", obj, O[pg].next);
	if (O[pg].next == 0) // add this page back to the free list
		page_push(pg, &objm_root);
	O[obj].next = O[pg].next;
	O[pg].next = obj;
}

void objm_debug(const char *filename)
{
#define FOUT(fmt, ...)   do{snprintf(buffer, 128, fmt, ## __VA_ARGS__); fputs(buffer, file);}while(0)
	char buffer[128];
	if (!filename)
		return;
	FILE *file = fopen(filename, "w");
	
	FOUT("digraph {\n");
	FOUT("node [shape=record];\n");
	FOUT("rankdir=LR;\n");
	int nfree(ObjRef pg) {
		int free = 0;
		ObjRef x = O[pg].next;
		while(x) {
			++free;
			x = O[x].next;
		}
		return free;
	}
	int is_leaf(ObjRef pg) {
		for (int i=0; i < N_SUBPAGES; ++i)
			if (O[pg].sub[i])
				return 0;
		return 1;
	}
	
	void dump_page(ObjRef pg) {
		//~ if (is_leaf(pg))
			//~ return;
		FOUT("struct%u [label=\"<root> %u :  [%d free @%u] _%d|{", pg, pg/64, nfree(pg), O[pg].next, O[pg].depth);
	
		for (int i = 0; i < N_SUBPAGES; ++i) {
			if (i) FOUT("|");
			FOUT("<f%d> %u", i, O[pg].sub[i]);
		}
		
		FOUT("}\"];\n");
		
		for (int i = N_SUBPAGES-1; i >= 0; --i) {
			if (O[pg].sub[i]) {
				FOUT("struct%u:f%d -> struct%u:root\n", pg, i, O[pg].sub[i]);
				
			}
		}
	}
	for (int i=0; i < npages; ++i)
		dump_page(i*OBJ_PER_PAGE);

	FOUT("root [label=\"%u\"];\n", objm_root);
	FOUT("root -> struct%u:root;\n", objm_root);
	FOUT("}\n");
	fclose(file);
	
}

#endif
#endif
