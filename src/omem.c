/**
32 bit address space (ORef) indexes every 8 bytes sizeof(TinyObj)
Every object starts with a 1 byte type which indexes into g_class[type] to get out-of-band information (size/parents/etc.)
Type 0 is used for free objects.
Type 1 is used for house-keeping objects.
Types 2~255 are user defined.

Memory allocation is done primarily by pages.
The paging system (mmap) is used so that individual pages can be freed, while still keeping a linear space.

The first page is special, it holds bookkeeping information.
The first PAGE_SIZE_BITS f16 objects are sentinals for the DLL free lists for each sized object.
After the sentinals the rest of the page is used as the root page in the free-page-list.

The free-page-list is a SLL of pages that hold a stack of 32bit PageObjRefs to pages that are free.
The reference to the next page in the SLL is at the root of the page in the FreeObj8.next space.
The reference to the next page actually points to the page AFTER to facilitate easy entry into the top of the previous(actual) page.
The first 8 bytes of the page are for the f8 object information (type=1, size=12, next=(PageObjRef)nextpage+1).
After the first 8 bytes the stack of PageObjRefs proceeds upwards.  And pages are removed top to bottom.
The global g_free_page is a (PageObjRef*) pointer to the top element of this stack.

g_free_page_root points somwhere into page0 where the stack starts.  
The first PageObjRef (*g_free_page_root) is a free page, but so are all of the pages above it.
The first PageObjRef (*g_free_page_root) will not be popped from the stack upon allocation, simply incremented.

TODO:
  sweep page free list to truncate tail pages and shrink *g_free_page_root
  sweep tiny obj free list to combine terms
  extendable objects with secondary free lists
  
*/
#ifndef OMEM_C
#define OMEM_C

#include <stdint.h>
#include <stdio.h>

#define PAGE_SIZE_BITS 12
#define OM_OPP (1<<(PAGE_SIZE_BITS-3))
#define FLAG_HALF_FREE 0x1

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
	uint8_t size;
	uint16_t flags;
	ORef next; // f8
};

struct s_FreeObj16 {
	uint8_t type;
	uint8_t flags;
	uint16_t size;
	FreeObjRef next; // OM.f16
	FreeObjRef prev; // OM.f16
	uint32_t pad;
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

union OM {
	PageObj *p;
	TinyObj *o;
	FreeObj8 *f8;
	FreeObj16 *f16;
	ORef *r;
};

void omem_init(uint32_t pages);
void omem_fini(void);
void omem_register_class(uint8_t type, ObjClass *kls);

PageObjRef omem_alloc_page(void);
void omem_free_page(PageObjRef page);

ORef omem_alloc(int size);
void omem_free(ORef obj);

int omem_page_in_use(ORef obj);

extern union OM OM;
extern PageObjRef *g_free_page, *g_free_page_root;

#if __INCLUDE_LEVEL__ == 0

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "logging.c"

union OM OM;
ObjClass g_class[256] = {0};
uint32_t g_max_pages;
PageObjRef *g_free_page_root, *g_free_page;

void omem_init(uint32_t pages)
{
	ASSERT(sizeof(PageObj) == 1<<PAGE_SIZE_BITS, "PageObj not right size");
	g_max_pages = pages;
	OM.p = mmap(0, g_max_pages*sizeof(PageObj), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
	ASSERT(OM.p != MAP_FAILED, "Can't mmap OM %dMbytes", g_max_pages*sizeof(PageObj)/1024/1024);
	
	OM.f8[0].type = 1;   // It is classified as internal mem.  But it is also our NULL object
	OM.f8[0].size = PAGE_SIZE_BITS;
	g_free_page = g_free_page_root = (PageObjRef*)&OM.f16[PAGE_SIZE_BITS];
	*g_free_page = 1;
	
	// initialize sentenials for object sizes
	for (int s = 4; s < PAGE_SIZE_BITS; ++s) {
		OM.f16[s*1].type = 1;
		OM.f16[s*1].size = 4;
		OM.f16[s*1].next = OM.f16[s*1].prev = s*1; // primary list
	}
	OM.f16[3*1].type = 1;
	OM.f16[3*1].size = 3;
	OM.f16[3*1].next = 3*1;
	

}

void omem_fini(void)
{
	ASSERT(munmap(OM.p, g_max_pages * sizeof(PageObj)) == 0, "Failed to unmap object mem");
}



ORef omem_alloc_page(void)
{
	PageObjRef page;
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
	XINFO("new page %d", page);
	return page;	
}

void omem_free_page(PageObjRef page)
{
	XINFO("Free page %d", page);
	++g_free_page;
	// add this object to the free list
	if ( (g_free_page - OM.r)%(OM_OPP*2) == 0) { 
		// this list is full, so use this object as the new list
		OM.p[page].f8[0].type = 1;
		OM.p[page].f8[0].size = PAGE_SIZE_BITS;
		OM.p[page].f8[0].next = (PageObj*)g_free_page - OM.p;
		g_free_page = &OM.p[page].f8[0].next;
		
	} else {
		*g_free_page = page;
		OM.p[page].f8[0].type = 0;
		ASSERT(madvise(&OM.p[page], sizeof(PageObj), MADV_DONTNEED) == 0, "madvise failed");
	}
}

ORef omem_alloc(int size)
{
	ORef obj;
	XXINFO("alloc size %d : %d", size, 1<<size);
	if (size == PAGE_SIZE_BITS) {
		obj = omem_alloc_page() * OM_OPP;
		
	} else if (size == 3) {
		if (OM.f16[3*1].next == 3*1) { // no more, split an 8byte obj
			obj = omem_alloc(4);
			XXINFO("Split 8byte: %x", obj);
			// free the friend
			OM.f8[obj+1].type = 0;
			OM.f8[obj+1].flags = 0; // fully free
			OM.f8[obj+1].size = 3;
			OM.f8[obj+1].next = OM.f16[3*1].next;
			OM.f16[3*1].next = obj+1;
			
		} else {
			obj = OM.f16[3*1].next;
			OM.f16[3*1].next = OM.f8[obj].next;
			XXINFO("Try %x: friend %x (%d:%d:%d)", obj, obj^1, OM.f8[obj^1].type, OM.f8[obj^1].flags, OM.f8[obj^1].size);
			// before using obj, see if its friend is free
			if (OM.f8[obj^1].type == 0) { // friend is also free so don't use this obj
				OM.f8[obj].flags |= FLAG_HALF_FREE; // we are still free but not in the free list
				if (OM.f8[obj^1].flags & FLAG_HALF_FREE) {
					// our friend is also half-free (not in the free list)
					// go ahead and free this whole 16bit block
					obj &= ~1;
					OM.f8[obj].type = 1;
					OM.f8[obj].size = 4;
					XXINFO("Free them both");
					omem_free(obj);
				}
				// go get a new one
				obj = omem_alloc(3);
			}
		}
		
	} else if (OM.f16[size*1].next == size*1) {
		XXINFO("No free objs of size %d, alloc new one", size);
		ORef big = omem_alloc(size+1) >> 1;
		// add the frind to the primary list
		obj = big + (1<<(size - 4));
		OM.f16[obj].size = size;
		OM.f16[obj].type = 0;
		OM.f16[obj].next = OM.f16[size*1].next;
		OM.f16[obj].prev = size*1;
		OM.f16[OM.f16[obj].next].prev = obj;
		OM.f16[size*1].next = obj;
		obj = big << 1;
		
	} else {
		XXINFO("Grab one from the list %d -> %d", size, OM.f16[size].next);
		// Just grab it from the list
		FreeObj16 *fo = &OM.f16[OM.f16[size*1].next];
		OM.f16[fo->prev].next = fo->next;
		OM.f16[fo->next].prev = fo->prev;
		obj = (fo - OM.f16) << 1;
	}
	return obj;
}


//~ ORef omem_alloc(uint8_t type)
//~ {
	//~ XINFO("ALLOC %x : %d", type, g_class[type].size);
	
	//~ int size = g_class[type].size;
	//~ ORef obj = omem_alloc_size(size);
	//~ memset(&OM.o[obj], 0, 1<<size);
	//~ OM.f8[obj].size = size;
	//~ OM.f8[obj].type = type;
	//~ return obj;
//~ }

void omem_free(ORef obj)
{
	// Free linked objects first somehow
	FreeObj8 *o8 = &OM.f8[obj];
	int size = (o8->type==1)? o8->size: g_class[o8->type].size;
	ASSERT(size != 0, "Double free");
	XINFO("FREEE: %x by %x:%d (%d)", obj, o8->type, size, (1<<size)/8);
	
	if (size == 3) { // special rules for freeing an 8byte object
		o8->type = 0; // it is now marked as free
		o8->size = 3; // free an 8 byte object
		o8->flags = 0; // make sure our FLAG_HALF_FREE is cleared
		// link it into the primary free list
		o8->next = OM.f16[3*1].next; // this next points to f8 not f16
		OM.f16[3*1].next = obj;
		
	} else if (size == PAGE_SIZE_BITS) { // free an entire page
		omem_free_page(obj / OM_OPP);
		
	} else { // is our friend already free?
		obj >>= 1; // we are FreeObjRef now
		FreeObjRef f = obj >> (size-4);
		f += (f & 0x1) ? -1 : 1;
		f <<= (size - 4);
		XXINFO("    we are %x, friend is %x, %d:%d", obj<<1, f<<1, OM.f16[f].type, OM.f16[f].size);
		
		if (OM.f16[f].type == 0 && OM.f16[f].size == size) {
			XXINFO("     Remove friend from list and free up");
			// our friend is also free so remove friend from list and free the larger block
			OM.f16[OM.f16[f].prev].next = OM.f16[f].next;
			OM.f16[OM.f16[f].next].prev = OM.f16[f].prev;
			f = (obj < f)? obj: f;
			OM.f16[f].size = size + 1;
			OM.f16[f].type = 1;
			omem_free(f<<1);
			
		} else {
			XXINFO("     Friend not free");
			// our friend is not free, so add this object to the primary free list
			FreeObj16 *f16 = &OM.f16[obj];
			f16->type = 0;
			f16->size = size;
			f16->prev = size*1; // primary sentinal
			f16->next = OM.f16[size*1].next;
			OM.f16[f16->next].prev = obj;
			OM.f16[size*1].next = obj;
		}
		
	}
}

void omem_register_class(uint8_t type, ObjClass *kls)
{
	memcpy(&g_class[type], kls, sizeof(ObjClass));
}

int omem_page_in_use(PageObjRef page)
{
	//~ PageObjRef page = obj / OM_OPP;
	if (page >= *g_free_page_root)
		return -2;
	if (page == 0)
		return -1;
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

void omem_gc(void)
{
	
	
	
}


#endif
#endif
