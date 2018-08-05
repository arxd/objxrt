#include <time.h>
#include <stdio.h>
#include "omem.c"
#include "logging.c"

void test_alloc(void)
{
	INFO("%d bytes / tiny", sizeof(TinyObj));
	INFO("%d bytes / med", sizeof(MedObj));
	INFO("%d bytes / big,  %d objs/big", sizeof(BigObj), OM_TPB);
	
	
	omem_init(2);
	ObjID x;
	int nobj = 0;
	while ( (x=omem_allocB())) {
		INFO("%d", x);
		++nobj;
	}
	INFO("%d objs allocated", nobj);
	omem_fini();
}

//~ void test_rand_alloc_free(void)
//~ {
	//~ bobj_init(1024*1024);
	//~ srand(time(0));
	//~ ObjID objs[1024] = {0};
	//~ for (int i=0; i < 200000; ++i) {
		//~ int z = rand()%1024;
		//~ if (objs[z]) {
		//	INFO("Free %x", objs[z]);
			//~ bobj_free(objs[z]);
			//~ objs[z] = 0;
		//~ } else {
			//~ objs[z] = bobj_alloc();
			//INFO("Alloc %x", objs[z]);
		//~ }
	//~ }
	
	//~ bobj_fini();
//~ }

//~ void dump(ObjID me)
//~ {
	//~ ObjID pg = me;
	//~ while (pg) {
		//~ for (int i=0; i < 128; ++i) {
			//~ for (int j =0; j < 8; ++j)
				//~ printf("%9d ", g_objmem[pg][i*8+j]);
			//~ printf("\n");
		//~ }
		//~ printf("--------------\n");
		//~ pg = g_objmem[pg][OPP-1];
	//~ }
	//~ fflush(stdout);
//~ }

//~ void test_names(void)
//~ {
	//~ bobj_init(1024*1024);
	
	//~ ObjID me = bobj_alloc();
	
	//~ ASSERT(bobj_get_istr(me, 0) == 0, "found?");
	
	//~ bobj_set_istr(me, 42, 1001);
	//~ ASSERT(bobj_get_istr(me, 0) == 0, "found?");
	//~ ASSERT(bobj_get_istr(me, 42) == 1001, "not found?");
	
	//~ for (int i=100; i < 1000; ++i) {
		//~ bobj_set_istr(me, i, i+10000);
	//~ }
	
	//~ for (int i=0; i < 232; ++i) 
		//~ bobj_ins_idx(me, -1, 2000+i);
	//~ bobj_ins_idx(me, 0, 3000);
	//~ bobj_ins_idx(me, 0, 3001);
	//~ bobj_ins_idx(me, 0, 3002);
	//~ bobj_ins_idx(me, 0, 3003);
	//~ bobj_parent_first(me, 1111);
	//~ bobj_parent_first(me, 2222);
	//~ bobj_parent_last(me, 3333);
	
	//~ bobj_ins_idx(me, 1, 4001);
	//~ bobj_ins_idx(me, 1, 4001);
	//~ bobj_ins_idx(me, 1, 4001);
	//~ bobj_ins_idx(me, 1, 4001);
	//~ bobj_ins_idx(me, 1, 4001);
	//~ bobj_ins_idx(me, -2, 9999);

	//~ ASSERT(bobj_get_idx(me, 0) == 3003, "bad get");
	
	//~ dump(me);
	
	
	//~ for (int i=1000-1; i >= 100; --i) {
		//~ ASSERT(bobj_get_istr(me, i) == i+10000, "bummer %d, %d", i, bobj_get_istr(me,i));
	//~ }
	
	//~ bobj_set_utf8(me, "aaron", 1234);
	//~ ASSERT(bobj_get_utf8(me, "aaron") == 1234, "aaron");
	
	
	//~ bobj_fini();
//~ }

//~ void test_parents(void)
//~ {
	//~ bobj_init(1024);
	
	//~ ObjID pp = bobj_alloc();
	//~ bobj_set_utf8(pp, "pp", -100);
	//~ bobj_set_utf8(pp, "name", -999);
	
	//~ ObjID p1 = bobj_alloc();
	//~ bobj_parent_first(p1, pp);
	//~ bobj_set_utf8(p1, "p1", -101);
	//~ bobj_set_utf8(p1, "p", -111);
	
	//~ ObjID p2 = bobj_alloc();
	//~ bobj_parent_first(p2, pp);
	//~ bobj_set_utf8(p2, "name", -888);
	//~ bobj_set_utf8(p2, "p2", -102);
	//~ bobj_set_utf8(p2, "p", -222);
	
	//~ ObjID child = bobj_alloc();
	//~ bobj_parent_first(child, p1);
	//~ bobj_parent_last(child, p2);
	//~ bobj_set_utf8(child, "x", -1234);
	//~ bobj_ins_idx(child, -1, (ObjID)-1000);
	//~ bobj_ins_idx(child, -1, (ObjID)-2000);
	
	//~ ASSERT(bobj_get_utf8(p1, "name") == -999, "p1.name");
	//~ ASSERT(bobj_get_utf8(p2, "name") == -888, "p2.name");
	//~ ASSERT(bobj_get_utf8(child, "name") == -999, "child.name = %d", bobj_get_utf8(child,"name"));
	//~ ASSERT(bobj_get_utf8(child, "y") == 0, "child.y");
	//~ ASSERT(bobj_get_utf8(child, "pp") == -100, "child.pp");
	//~ ASSERT(bobj_get_utf8(child, "p") == -111, "child.p");
	
	
	//~ bobj_graphviz("objtest.dot");
	
	//~ bobj_fini();
//~ }

int main(int argc, char *argv[])
{
	test_alloc();
	//~ test_rand_alloc_free();
	//~ test_names();
	//~ test_parents();

	INFO("All tests passed!");
	return 0;
}