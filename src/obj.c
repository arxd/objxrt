/**

small_obj - 64 bytes
tiny_obj = 8 bytes



*/
#ifndef OBJ_C
#define OBJ_C

#include <stdint.h>

#define PAGE_SIZE 4096

typedef uint32_t ObjID;

void obj_init(void);
void obj_fini(void);


#if __INCLUDE_LEVEL__ == 0

#endif
#endif
