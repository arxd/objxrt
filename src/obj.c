/**

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
