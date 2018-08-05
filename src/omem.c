/**
32 bit address space (ObjID) indexs 8byte tiny objs
29 bits of small objects (64 bytes)

small_obj - 64 bytes
tiny_obj = 8 bytes

tiny obj:
1xxxxxxxxxxxx... = 63bit double number
XXXXtttttttttttt = 32k classes (X) with 6 bytes of payload (t)
0000.......      = small_obj

small_obj:

starts with 2 bytes of zero.  
The next 4 bytes are an ObjID of the zeroth parent.  It should describe the rest of the bytes.
Then 58 bytes of object-specific data.

*/
#ifndef OMEM_C
#define OMEM_C

#include <stdint.h>

#define PAGE_SIZE 4096
#define OM_TPB 512
#define OM_OPB 1024
#define OM_MPB 64
#define OM_OPM 16
#define OM_TPM 8

typedef struct s_FreeList FreeList;
typedef union u_MedObj MedObj;
typedef union u_BigObj BigObj;
typedef uint64_t TinyObj;
typedef uint32_t ObjID;

union u_MedObj {
	TinyObj t[OM_TPM];
	ObjID o[OM_OPM];
};

union u_BigObj {
	MedObj m[OM_MPB];
	TinyObj t[OM_TPB];
	ObjID o[OM_OPB];
};



struct s_FreeList {
	ObjID _[OM_OPB];
};

void omem_init(uint32_t pages);
void omem_fini(void);

ObjID omem_allocB(void);

#if __INCLUDE_LEVEL__ == 0

#include <unistd.h>
#include <string.h>
#include <sys/mman.h>
#include "logging.c"

union {
	BigObj *b;
	MedObj *m;
	TinyObj *t;
	ObjID *o;
	void *p;
} OM;

int g_max_pages;

ObjID *g_Bfree;
ObjID *g_Mfree;
ObjID *g_Sfree;

void omem_init(uint32_t pages)
{
	ASSERT(sizeof(BigObj) == PAGE_SIZE, "BigObj not right size");
	g_max_pages = pages;
	OM.p = mmap(0, g_max_pages*PAGE_SIZE, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
	ASSERT(OM.p != MAP_FAILED, "Can't mmap OM");
	g_Bfree = OM.o;
	g_Bfree[0] = OM_TPB;
}

void omem_fini(void)
{
	ASSERT(munmap(OM.p, g_max_pages * PAGE_SIZE) == 0, "Failed to unmap object mem");
}


ObjID omem_allocB(void)
{
	ObjID obj;
	if ((uint64_t)g_Bfree % PAGE_SIZE == 0) { // this free list is empty
		if (g_Bfree == OM.o) { 
			// No more free lists.  Grab the page from the end (stored at the root)
			obj = g_Bfree[0];
			if (((BigObj*)&OM.t[obj] - OM.b) == g_max_pages)
				return 0; // out of memory
			g_Bfree[0] += OM_TPB;
		} else {
			// there is another page of free pages.
			// use this page as the object and follow the link
			obj = (TinyObj*)g_Bfree - OM.t;
			g_Bfree = (ObjID*)&OM.t[*g_Bfree] - 1;
		}
	} else {
		// just a normal free page
		obj = *g_Bfree;
		--g_Bfree;
	}
	memset(&OM.t[obj], 0, sizeof(BigObj));
	return obj;	
}



#endif
#endif
