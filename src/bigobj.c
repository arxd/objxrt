/**
The memory layout of a default object is:

------------
num_of_parent + array objs (2046)
num_of_array_objs (2043)
parent0
parent1
parent2
obj[0]
obj[1]
...
obj[1017]
NEXT_PAGE
-----------
obj[1018]
...
obj[2040]
NEXT_PAGE
------
obj[2041]
obj[2042]
NULL padding
name/obj [odd]
name/obj
name/obj
name/obj
...
NULL
==========

one page per object.  If an object is larger than a page then it overflows onto another page.

The free pages are recorded in a free list (linked pages starting at page 0)


Next_free_page or NULL
freePage
freePage  <-- g_free_top
*


*/
#ifndef BIGOBJ_C
#define BIGOBJ_C

#include <stdint.h>
#include "intstr.c"
#include "obj.c"

#define PAGE_SIZE 4096
#define OPP 1024

typedef ObjID Bobject[OPP];
typedef  struct s_ObjIter ObjIter;
typedef struct s_SlotIter SlotIter;

//~ #define MAX_OBJECTS (1024*1024)
#define MAX_LINKS (32*1024*1024)

struct s_ObjIter {
	ObjID obj;
};

struct s_SlotIter {
	Bobject *page0, *page;
	ObjID objid;
	int i, page_i, bobj_i;
};

extern Bobject *g_bobjmem;

void bobj_init(uint32_t max_objects);
void bobj_fini(void);

ObjID bobj_alloc(void);
void bobj_free(ObjID obj);

ObjID bobj_get_idx(ObjID obj, int index);
ObjID bobj_get_utf8(ObjID obj, const char *key_utf8);
ObjID bobj_get_istr(ObjID obj, IStr key_istr);

ObjID bobj_set_idx(ObjID obj, int index, ObjID val);
ObjID bobj_ins_idx(ObjID obj, int index, ObjID val);
ObjID bobj_set_utf8(ObjID obj, const char *key_utf8, ObjID val);
ObjID bobj_set_istr(ObjID obj, IStr key_istr, ObjID val);


ObjID bobj_parent_at(ObjID obj, int index, ObjID parent);
ObjID bobj_parent_first(ObjID obj, ObjID parent);
ObjID bobj_parent_last(ObjID obj, ObjID parent);

int bobj_length(ObjID self);
void bobj_graphviz(const char *filename);

void iter_objs_init(ObjIter *iter);
int iter_objs_next(ObjIter *iter);

void iter_parents_init(SlotIter *iter, ObjID obj);
int iter_parents_next(SlotIter *iter);

void iter_list_init(SlotIter *iter, ObjID obj);
int iter_list_next(SlotIter *iter);

void iter_keys_init(SlotIter *iter, ObjID obj);
int iter_keys_next(SlotIter *iter);


#if __INCLUDE_LEVEL__ == 0

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <sys/mman.h>
#include "logging.c"

Bobject *g_bobjmem;
static ObjID *g_free_top;
static uint32_t g_max_objects;

/* ==============- Initialization -==================
*/
void bobj_init(uint32_t max_objects)
{
	g_max_objects = max_objects;
	ASSERT(sizeof(Bobject) == PAGE_SIZE, "%d", sizeof(Bobject)); 
	g_bobjmem = mmap(0, g_max_objects*sizeof(Bobject), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
	ASSERT(g_bobjmem != MAP_FAILED, "Can't mmap %dMbytes for big objects", g_max_objects*sizeof(Bobject)/1024/1024);
	g_free_top = (ObjID*)g_bobjmem;
	g_free_top[0] = 1;
}

void bobj_fini(void)
{
	ASSERT(munmap(g_bobjmem, g_max_objects*sizeof(Bobject)) == 0, "Failed to unmap object mem");
}

ObjID bobj_alloc(void)
{
	ObjID obj;
	if((uint64_t)g_free_top % PAGE_SIZE == 0) { // this free list is empty
		if (g_free_top == (ObjID*)g_bobjmem) { 
			// this is the root free list.  Grab the page from the end
			obj = g_free_top[0];
			if (g_free_top[0] == g_max_objects)
				return 0; // out of memory
			g_free_top[0] ++;
		} else {
			// there is another page of free pages.
			// use this page as the object and follow the link
			obj = (Bobject*)g_free_top - g_bobjmem;
			g_free_top = (ObjID*)&g_bobjmem[*g_free_top] - 1;
		}
	} else {
		// just a normal free page
		obj = *g_free_top;
		--g_free_top;
		
	}
	memset(&g_bobjmem[obj], 0, PAGE_SIZE);
	return obj;
}

void bobj_free(ObjID obj)
{
	if (g_bobjmem[obj][OPP-1])
		bobj_free(g_bobjmem[obj][OPP-1]);
	++g_free_top;
	// add this object to the free list
	if ((uint64_t)g_free_top%PAGE_SIZE == 0) {
		// this list is full, so use this object as the new list
		g_bobjmem[obj][0] = (Bobject*)g_free_top - g_bobjmem;
		g_free_top = &g_bobjmem[obj][0];
	} else {
		*g_free_top = obj;
		ASSERT(madvise(&g_bobjmem[obj], PAGE_SIZE, MADV_DONTNEED) == 0, "madvise failed");
	}
}

/* ==============- Helper -==================
*/

static ObjID *jump_to(Bobject **page, int *index)
{
	int pg = 0, ofst;
	int idx = *index;
	*index = idx+2;
	if (idx >= OPP-3) {
		// beyond the first page
		idx -= OPP-3;
		pg = idx / (OPP-1) + 1;
		*index = idx % (OPP-1);
	}
	while (pg -- )
		*page = &g_bobjmem[(**page)[OPP-1]];
	return &(**page)[*index];
}

static int names_search(Bobject **page, int *index, IStr key)
{
	while ((**page)[*index]) {
		if (*index == OPP - 1) {
			*page = &g_bobjmem[(**page)[*index]];
			*index = 1;
		}
		if ((**page)[*index] == key)
			return 1;
		*index += 2;
	}
	return 0;
}


static void insert_at(ObjID obji, int index, ObjID value)
{
	Bobject *page = &g_bobjmem[obji];
	int end = g_bobjmem[obji][0];
	int idx = index;
	
	jump_to(&page, &idx);
	while (index < end) {
		ObjID tmp = (*page)[idx];
		(*page)[idx] = value;
		value = tmp;
		// advance idx/index
		++index;
		++idx;
		if (idx == OPP-1) { // go to next page
			if ((*page)[idx] == 0)
				(*page)[idx] = bobj_alloc();
			page = &g_bobjmem[(*page)[idx]];
			idx = 0;
		}
	}
	
	ASSERT(g_bobjmem[obji][0] < MAX_LINKS, "Out of link memory!");
	g_bobjmem[obji][0] += 1;
	
	if ((*page)[idx]) { // Need to move a name/val pair to make room
		bobj_set_istr(obji, (*page)[idx], (*page)[idx+1]);
		(*page)[idx+1] = 0;
	}
	(*page)[idx] = value;
}

/* ==============- Parent -==================
*/

ObjID bobj_parent_at(ObjID obj, int index, ObjID parent)
{
	int num = g_bobjmem[obj][0] - g_bobjmem[obj][1];
	if (index < 0)
		index += num+1;
	ASSERT(index >= 0 && index <= num, "index out of bounds 0 <= %d [%d]", num, index);
	insert_at(obj, index, parent);
	return obj;
}

ObjID bobj_parent_first(ObjID obj, ObjID parent)
{
	return bobj_parent_at(obj, 0, parent);
}

ObjID bobj_parent_last(ObjID obj, ObjID parent)
{
	return bobj_parent_at(obj, -1, parent);
}

/* ==============- Getters -==================
*/

ObjID bobj_get_utf8(ObjID obj, const char *key_utf8)
{
	return bobj_get_istr(obj, is_get(key_utf8));
}

ObjID _bobj_get_istr(ObjID obj, IStr key)
{
	Bobject *page = &g_bobjmem[obj];
	int idx = (int)(*page)[1];
	jump_to(&page, &idx);
	if (idx % 2 == 0)
		idx++;	
	if (names_search(&page, &idx, key))
		return (*page)[idx+1];
	return 0;
}

/** This gets a slot.  If the lookup fails on obj then the lookup continues depth first on each parent
*/
ObjID bobj_get_istr(ObjID obj, IStr key_istr)
{
	ObjID val = _bobj_get_istr(obj, key_istr);
	if (val)
		return val;
	int nparents = g_bobjmem[obj][0] - g_bobjmem[obj][1];
	Bobject *page = &g_bobjmem[obj];
	int idx = 2;
	while (nparents--) {
		val = bobj_get_istr((*page)[idx], key_istr);
		if (val)
			return val;
		idx++;
		if (idx == OPP-1) {
			page = &g_bobjmem[(*page)[idx]];
			idx = 0;
		}
	}
	return 0;
}

ObjID bobj_get_idx(ObjID obj, int index)
{
	Bobject *page = &g_bobjmem[obj];
	if (index < 0)
		index += g_bobjmem[obj][1];
	if (index < 0 || index >= g_bobjmem[obj][1])
		return 0; // out of bounds
	index += g_bobjmem[obj][0] - g_bobjmem[obj][1];
	return *jump_to(&page, &index);
}

/* ==============- Setters -==================
*/

/**
index -1 appends the object to the end (push)
*/
ObjID bobj_ins_idx(ObjID obj, int index, ObjID val)
{
	g_bobjmem[obj][1] ++;
	if (index < 0)
		index += g_bobjmem[obj][1];
	ASSERT(index >= 0 && index <= g_bobjmem[obj][1], "index out of bounds 0 <= %d [%d]", g_bobjmem[obj][1], index);
	insert_at(obj, g_bobjmem[obj][0] - (g_bobjmem[obj][1]-1) + index, val);
	return obj;
}

ObjID bobj_set_idx(ObjID obj, int index, ObjID val)
{
	Bobject *o = &g_bobjmem[obj];
	if (index < 0)
		index += g_bobjmem[obj][1];
	ASSERT(index >= 0 && index < g_bobjmem[obj][1], "index out of bounds 0 < %d  [%d]", 0, g_bobjmem[obj][1], index);
	ObjID *x = jump_to(&o, &index);
	*x = val;
	return obj;
}

ObjID bobj_set_utf8(ObjID obj, const char *key_utf8, ObjID val)
{
	return bobj_set_istr(obj, is_get(key_utf8), val);
}

ObjID bobj_set_istr(ObjID obj, IStr key_istr, ObjID val)
{
	Bobject *page = &g_bobjmem[obj];
	int idx = g_bobjmem[obj][0]; 
	jump_to(&page, &idx);
	if (idx % 2 == 0)
		idx++;	
	if (!names_search(&page, &idx, key_istr)) {
		if (idx == OPP - 1) { // need some more memory
			(*page)[idx] = bobj_alloc();
			page = &g_bobjmem[(*page)[idx]];
			idx = 1;
		}
		(*page)[idx] = key_istr;
	}
	(*page)[idx+1] = val;
	return obj;
}

/* ==============- Other -==================
*/

int bobj_length(ObjID obj)
{
	return g_bobjmem[obj][1];
}


	

int is_bobj_page(ObjID pageid)
{
	ObjID *top = g_free_top;
	while (1) {
		if (pageid == *top)
			return 0; // this is a free page
		
		if ( (uint64_t)top % PAGE_SIZE == 0) {
			if ( (Bobject*)top - g_bobjmem == pageid)
				return 0; // this is a page for free lists
			if ( (Bobject*)top == g_bobjmem) {
				if (pageid >= *top)
					return 0; // this is a free page
				return 1;
			}
			top = (ObjID*)&g_bobjmem[*top] - 1;
		} else {
			--top;
		}
	}
}


void iter_objs_init(ObjIter *iter)
{
	iter->obj = 0;
}

int iter_objs_next(ObjIter *iter)
{
	do {
		++iter->obj;
		if (iter->obj >= g_bobjmem[0][0])
			return 0;
	} while (!is_bobj_page(iter->obj));
	return 1;
}

void iter_parents_init(SlotIter *iter, ObjID obj)
{
	iter->page = iter->page0 = &g_bobjmem[obj];
	iter->i = iter->bobj_i = -1;
	iter->page_i = 1;
	iter->objid = 0;
}

int iter_parents_next(SlotIter *iter)
{
	++iter->bobj_i;
	++iter->i;
	++iter->page_i;
	if (iter->i >= ((*iter->page0)[0] - (*iter->page0)[1]))
		return 0;
	if (iter->page_i == OPP-1) {
		iter->page = &g_bobjmem[(*iter->page)[OPP-1]];
		iter->page_i = 0;
	}
	iter->objid = (*iter->page)[iter->page_i];
	return 1;
}


void iter_list_init(SlotIter *iter, ObjID obj)
{
	iter->page = iter->page0 = &g_bobjmem[obj];
	iter->i = -1;
	iter->bobj_i = iter->page_i = g_bobjmem[obj][0] - g_bobjmem[obj][1];
	iter->objid = *jump_to(&iter->page, &iter->page_i);
}

int iter_list_next(SlotIter *iter)
{
	++ iter->i;
	if (iter->i >= (*iter->page0)[1])
		return 0;
	if (iter->i == 0)
		return 1;
	++ iter->bobj_i;
	++ iter->page_i;
	if (iter->page_i == OPP-1) {
		iter->page = &g_bobjmem[(*iter->page)[OPP-1]];
		iter->page_i = 0;
	}
	iter->objid = (*iter->page)[iter->page_i];
	return 1;
}

void iter_keys_init(SlotIter *iter, ObjID obj)
{
	iter->page = iter->page0 = &g_bobjmem[obj];
	iter->i = -1;
	iter->bobj_i = iter->page_i = g_bobjmem[obj][0];
	jump_to(&iter->page, &iter->page_i);
	if (iter->page_i % 2 == 0)
		++iter->page_i;
	iter->objid = (*iter->page)[iter->page_i];
	iter->page_i -= 2;
}

int iter_keys_next(SlotIter *iter)
{
	++ iter->i;
	iter->page_i += 2;
	if ((*iter->page)[iter->page_i] == 0)
		return 0;
	if (iter->page_i == OPP-1) {
		iter->page = &g_bobjmem[(*iter->page)[OPP-1]];
		iter->page_i = 1;
	}
	iter->objid = (*iter->page)[iter->page_i];
	return 1;
}

void bobj_graphviz(const char *filename)
{
#define FOUT(fmt, ...)   do{snprintf(buffer, 128, fmt, ## __VA_ARGS__); fputs(buffer, file);}while(0)
	char buffer[128];
	
	FILE *file = fopen(filename, "w");
	
	FOUT("digraph {\n");
	FOUT("node [shape=record];\n");
	
	ObjIter itr;
	SlotIter sitr;
	iter_objs_init(&itr);
	while (iter_objs_next(&itr)) {		
		FOUT("N%u [label=\"<root> %u|{", itr.obj, itr.obj);
		int nparents = g_bobjmem[itr.obj][0] - g_bobjmem[itr.obj][1];
		iter_parents_init(&sitr, itr.obj);
		while(iter_parents_next(&sitr)) {
			if (sitr.i) FOUT("|");
			FOUT("<p%d> _p_%u", sitr.i, sitr.i);
		}
		
		iter_list_init(&sitr, itr.obj);
		while(iter_list_next(&sitr)) {
			if ((int)sitr.objid > 0) {
				FOUT("|<l%d> [%d]", sitr.i, sitr.i);
			} else {
				FOUT("|[%d] = %d", sitr.i, sitr.objid);
			}
		}
		
		iter_keys_init(&sitr, itr.obj);
		while(iter_keys_next(&sitr)) {
			ObjID link = (*sitr.page)[sitr.page_i+1];
			if ((int)link > 0) {
				FOUT("|<k%d> %s", sitr.i, is_utf8(sitr.objid));
			} else {
				FOUT("|%s = %d", is_utf8(sitr.objid), link);
			}
		}
		
		FOUT("}\"];\n");
	
		iter_parents_init(&sitr, itr.obj);
		while(iter_parents_next(&sitr))
			FOUT("N%u:p%d -> N%u:root\n", itr.obj, sitr.i, sitr.objid);
		
		iter_list_init(&sitr, itr.obj);
		while(iter_list_next(&sitr)) {
			if ((int)sitr.objid > 0)
				FOUT("N%u:l%d -> N%u:root\n", itr.obj, sitr.i, sitr.objid);
		}
	}
	
	//~ int64_t dump_node(uint32_t node_AVLR) {
		//~ INFO("Dump node %d\n", node_AVLR);
		//~ AVLNode *node = AVLP(self, node_AVLR);
		//~ if (node_AVLR == 0) {
			//~ FOUT("n%u [shape=circle, label=\"\", color=black];\n",  (uint32_t)(-(--null_counter)));
			//~ return null_counter;
		//~ }
		//~ FOUT("p%u [shape=circle, label=\"",(uint32_t)node->key);
		//~ if (func)
			//~ FOUT("%s\", color=", func(node->key, node->data));
		//~ else
			//~ FOUT("%u\", color=", (uint32_t)node->key);
		
		//~ switch(node->balance) {
			//~ case 1:FOUT("red];\n");break;
			//~ case 2:FOUT("black];\n");break;
			//~ case 3:FOUT("blue];\n");break;
			//~ default:FOUT("green];\n");break;
		//~ }
		
		//~ subkey = dump_node(node->sub[0]);
		//~ FOUT("\tp%u -> %c%u [color=red];\n", (uint32_t) node->key, subkey<0 ? 'n' : 'p', (uint32_t)(subkey<0 ? -subkey : subkey));
		//~ subkey = dump_node(node->sub[1]);
		//~ FOUT("\tp%u -> %c%u [color=black];\n", (uint32_t)node->key, subkey<0 ? 'n' : 'p', (uint32_t)(subkey<0 ? -subkey : subkey));
		//~ return node->key;
	//~ }
	//~ dump_node(self->sub0);
	FOUT("}\n");
	fclose(file);	
	

}

#endif
#endif
