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
Generic OBJ : 64b: 3b Nparents, 4b NUser, 4b UserRef, 4b NItems, 4b itemRef, 4b Nslots, 4b slotRef, 8x parent pointers, 4b parent linked



80~: 63bit double

02...06
07 : ext 4096b
08 : 



*/
#ifndef OBJ_C
#define OBJ_C

void obj_free(ObjID obj);

int obj_get_data(ObjID objid, void **ptr);
void obj_set_data(ObjID objid, int bytes, void **ptr);



#if __INCLUDE_LEVEL__ == 0

#include <unistd.h>
#include <sys/mman.h>
#include "logging.c"







#endif
#endif
