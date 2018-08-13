/**

1 byte
00 : unk : free
01 : 64b : linked mem
02......
07 : 4kb : linked mem
User Data : 8b  : 3b size, 4b Ref to user data
User Data Int : 8b : 7b int
User Data TinyUTF8: 8b : 7b string

Large Dict : 8b  : 1b nparents, 2b nslots, 4b Ref to ext || parents guaranteed to be in the first block
Large List : 8b  : 1b nparents, 2b nitems, 4b Ref to ext || parents in first block, max 64k array elements
Small Dict : 128b: 1b[2B nparents, 6B nslots], 2b??, 12b parents, 112b slots (max 14)
Small List : 64b: 1b[2B nparents, 6B nitems], 2b??, 60b els (max 15)


0: free objects
1: omem interal 
2: generic small object
2: generic medium object
3: generic large object

128~: double

*/
#ifndef OBJ_C
#define OBJ_C

#define TYPE_FREE 0
#define TYPE_OMEM 1
#define TYPE_LIST_IMBAL 2
#define TYPE_LIST_BAL 3
#define TYPE_LIST_END 4
#define TYPE_REDIRECT 5

#include "omem.c"
#include "intstr.c"

void obj_init(uint32_t npages);
void obj_fini(void);

void obj_new(ORef parent);

ORef obj_get_utf8(ORef obj, const char *key_utf8);
ORef obj_set_utf8(ORef obj, const char *key_utf8, ORef val);

ORef obj_get_idx(ORef obj, int idx);
ORef obj_set_idx(ORef obj, int idx, ORef val);
ORef obj_ins_idx(ORef obj, int idx, ORef val);

ORef obj_get_istr(ORef obj, IStr key_istr);
ORef obj_set_istr(ORef obj, IStr key_istr, ORef val);

void *obj_get_data(ORef obj);
void obj_set_data(ORef obj, void *data_ptr);

uint32_t obj_get_length(ORef obj);

#if __INCLUDE_LEVEL__ == 0

#include "logging.c"

void obj_init(uint32_t npages)
{
	omem_init(npages);
	is_init();
	
	// Setup default set of objects
	
	
}

void obj_fini(void)
{
	is_fini();
	omem_fini();
}


#endif
#endif
