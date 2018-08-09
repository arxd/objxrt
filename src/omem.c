/**
32 bit address space (ORef) indexs 8byte tiny objs
29 bits of small objects (64 bytes)

small_obj - 64 bytes
tiny_obj = 8 bytes

tiny obj:
1xxxxxxxxxxxx... = 63bit double number
XXXXtttttttttttt = 32k classes (X 0002~7fff) with 6 bytes of payload (t)
0002.......      = small_obj

small_obj:

starts with 2 bytes of zero.  
The next 4 bytes are an ORef of the zeroth parent.  It should describe the rest of the bytes.
Then 58 bytes of object-specific data.

the first page holds the DLL sentenals
each size from 3(8)~11(2048) gets two sentenals
After the sentinals, the rest of the page is dedicated to ORefs for free pages.
starting from ORef FREE_PAGE ROOT


*/
#ifndef OMEM_C
#define OMEM_C

#include <stdint.h>
#include <stdio.h>

#define PAGE_SIZE_BITS 12
#define OM_OPP (1<<(PAGE_SIZE_BITS-3))

typedef union u_PageObj PageObj;
typedef uint64_t TinyObj;
typedef struct s_FreeObj16 FreeObj16;
typedef struct s_FreeObj8 FreeObj8;
typedef struct s_ObjClass ObjClass;

typedef uint32_t ORef; // indexes every 8 bytes into OM.o
typedef uint32_t FreeObjRef; // indexes every 16 bytes into OM.f16
typedef uint32_t PageObjRef; // indexes every 16 bytes into OM.f16

struct s_FreeObj8 {
	uint8_t type;
	uint8_t flags;
	uint16_t size;
	ORef next; // f8
};

struct s_FreeObj16 {
	uint8_t type;
	uint8_t flags;
	uint16_t size;
	uint32_t age;
	FreeObjRef prev; // OM.f16
	FreeObjRef next; // OM.f16
};

struct s_ObjClass {
	int size; // in bits 1<<size (1=2, 2=4, 3=8)
	ORef parent;
	
};

union u_PageObj {
	FreeObj8 f8[OM_OPP];
	FreeObj16 f16[OM_OPP/2];
	TinyObj o[OM_OPP];
	ORef r[OM_OPP*2];
};

void omem_init(uint32_t pages);
void omem_fini(void);

ORef omem_alloc_page(void);
ORef omem_alloc(uint8_t type);
void dump_free_pages(void);

void omem_free_page(ORef page_obj);
int omem_page_in_use(ORef obj);

#if __INCLUDE_LEVEL__ == 0

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "logging.c"

union {
	PageObj *p;
	TinyObj *o;
	FreeObj8 *f8;
	FreeObj16 *f16;
	ORef *r;
} OM;



ObjClass g_class[256] = {
	{0, 0}, // free object
	{0, 0}, // internal_use
	{0, 0}, // user data
	{0, 0}, // extension
	{0}
	
};

uint32_t g_max_pages;
uint32_t g_num_objects;
uint32_t g_golden_offset;
PageObjRef *g_free_page_root, *g_free_page;

void omem_init(uint32_t pages)
{
	ASSERT(sizeof(PageObj) == 1<<PAGE_SIZE_BITS, "PageObj not right size");
	g_max_pages = pages;
	OM.p = mmap(0, g_max_pages*sizeof(PageObj), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
	ASSERT(OM.p != MAP_FAILED, "Can't mmap OM %dMbytes", g_max_pages*sizeof(PageObj)/1024/1024);
	
	g_num_objects = 1;        // the zeroth page is used
	OM.f8[0].type = 1;   // It is classified as free-page-list
	OM.f8[0].size = PAGE_SIZE_BITS;
	g_free_page = g_free_page_root = (PageObjRef*)&OM.f16[PAGE_SIZE_BITS*2];
	*g_free_page = 1;
	
	// initialize sentenials for object sizes
	for (int s = 3; s < PAGE_SIZE_BITS; ++s) {
		OM.f16[s*1].type = OM.f16[s*2].type = 1;
		OM.f16[s*1].size = OM.f16[s*2].size = 2;
		OM.f16[s*1].next = OM.f16[s*1].prev = (s*1 << 1); // primary list
		OM.f16[s*2].next = OM.f16[s*2].prev = (s*2 << 1); // secondary list
	}
		
}

void omem_fini(void)
{
	ASSERT(munmap(OM.p, g_max_pages * sizeof(PageObj)) == 0, "Failed to unmap object mem");
}



ORef omem_alloc_page(void)
{
	uint32_t page;
	if (g_free_page == g_free_page_root) {
		// no more free pages at all.  Grab the page from the end (stored at root position)
		page = *g_free_page;
		if (page == g_max_pages)
			return 0; // out of memory
		*g_free_page += 1;
	
	} else if ( (g_free_page - OM.r)%(OM_OPP*2) == 1) { 
		// this free list is empty
		// there is another page of free pages.
		// use this page as the object and follow the link
		page = (PageObj*)(g_free_page-1) - OM.p;
		g_free_page = (PageObjRef*)&OM.p[*g_free_page] - 1;

	} else {
		// just a normal free page
		page = *g_free_page;
		--g_free_page;
	}
	memset(&OM.p[page], 0, sizeof(PageObj));
	OM.p[page].f8[0].type = 1;
	OM.p[page].f8[0].size = PAGE_SIZE_BITS;
	return page * OM_OPP;	
}

void omem_free_page(ORef obj)
{
	//~ if (g_Bmem.t[obj][OPP-1])
		//~ omem_free(g_bobjmem[obj][OPP-1]);
	++g_free_page;
	// add this object to the free list
	if ( (g_free_page - OM.r)%(OM_OPP*2) == 0) { 
		// this list is full, so use this object as the new list
		OM.f8[obj].type = 1;
		OM.f8[obj].size = PAGE_SIZE_BITS;
		OM.f8[obj].next = (PageObj*)g_free_page - OM.p;
		g_free_page = &OM.f8[obj].next;
		
	} else {
		*g_free_page = obj / OM_OPP;
		OM.f8[obj].type = 0;
		//ASSERT(madvise(&OM.o[obj], sizeof(PageObj), MADV_DONTNEED) == 0, "madvise failed");
	}
}

ORef omem_alloc(uint8_t type)
{
}


void omem_free(ORef obj)
{
	// Free linked objects first somehow
	FreeObj8 *o8 = &OM.f8[obj];
	int size = g_class[o8->type].size;
	ASSERT(size != 0, "Double free");
	
	if (size == 3) { // special rules for freeing an 8byte object
		o8->type = 0; // it is now marked as free
		o8->size = 3; // free an 8 byte object
		o8->flags = 1; // only half free
		// link it into the primary free list
		o8->next = OM.f16[3*1].next;
		OM.f16[3*1].next = obj;
		
	} else if (size == PAGE_SIZE_BITS) { // free an entire page
		omem_free_page(obj);
		
	} else { // is our friend already free?
		obj >>= 1; // we are FreeObjRef now
		FreeObjRef f = obj >> (size-2);
		f += f & 0x1 ? -1 : 1;
		f <<= (size - 2);
		INFO("we are %x, friend is %x, size %d", obj, f, size);
		
		if (OM.f16[f].type == 0 && OM.f16[f].size == size) {
			// our friend is also free so remove friend from list and free the larger block
			OM.f16[OM.f16[f].prev].next = OM.f16[f].next;
			OM.f16[OM.f16[f].next].prev = OM.f16[f].prev;
			OM.f16[f].size = OM.f16[obj].size = size + 1;
			omem_free(f < obj? f<<1 : obj<<1);
			
		} else {
			// our friend is not free, so add this object to the primary free list
			FreeObj16 *f16 = &OM.f16[obj>>1];
			f16->type = 0;
			f16->size = size;
			f16->prev = size*1; // primary sentinal
			f16->next = OM.f16[size*1].next;
			OM.f16[f16->next].prev = obj;
			OM.f16[size*1].next = obj;
		}
		
	}
	
	
}

void dump_free_pages(void)
{
	PageObjRef *top = g_free_page;
	while (top != g_free_page_root) {
		printf("[%.4d:%d] ", *top, OM.p[*top].f8[0].type);
		fflush(stdout);
		if ( (top - OM.r)%(OM_OPP*2) == 1) {
			INFO("---------- %d ", *top - 1);
			top = (PageObjRef*)&OM.p[*top] - 1;
		} else {
			--top;
		}
	}

}

int omem_page_in_use(ORef obj)
{
	PageObjRef page = obj / OM_OPP;
	if (page >= *g_free_page_root)
		return -2;
	PageObjRef *top = g_free_page;
	while (top != g_free_page_root) {
		if ( (top - OM.r)%(OM_OPP*2) == 1) { 
			if ( (PageObj*)(top-1) - OM.p == page)
				return -1; // this is a page for free lists
			top = (PageObjRef*)&OM.p[*top];
		} else if (page == *top) {
			ASSERT(OM.p[page].f8[0].type == 0, "wrong type %d: %d", page, OM.p[page].f8[0].type);
			return 0; // this is a free page
		}

		--top;
	}
	return 1;	
}



#endif
#endif
