#include <time.h>
#include <stdio.h>
#include "omem.c"
#include "logging.c"


void dump_page_ruler(void)
{
	for (int o=0; o < OM_OPP; ++o) {
		if (o && o%8 == 0)
			printf("%2x", o%256);
		else
			printf(" ");
	}
	printf("|\n");
	fflush(stdout);
	
}

void dump_page(PageObjRef page)
{
	for (int o=0; o < OM_OPP; ++o) {
		if (o && o%8==0)
			printf(" ");
		printf("%x", OM.p[page].f8[o].type);
		int s = 1 << (OM.p[page].f8[o].size - 3);
		int free = (OM.p[page].f8[o].type == 0);
		while(--s && ++o) {
			if (o%8==0)
				printf(" ");
			printf("%c", free?'.':'-');
		}
	}
	printf("|\n");
	fflush(stdout);
}

void dump_page_freelist(PageObjRef page)
{
	ASSERT(OM.p[page].f8[0].type == 1, "wrong type");
	int curptr = (g_free_page - OM.r) / (2*OM_OPP);
	curptr = (curptr == page)? ((g_free_page - OM.r) >> 1)%OM_OPP : OM_OPP;
	for (int o = 0; o < OM_OPP; ++o) {
		if (o && o%8==0)
			printf(" ");
		if (o < curptr)
			printf("p");
		else if (o == curptr)
			printf("P");
		else
			printf(".");
	}
	printf("|\n");
	fflush(stdout);
	
}

void dump_pages(void)
{
	INFO("DUMP Pages");
	dump_page_ruler();
	int p, page=0;
	while ((p = omem_page_in_use(page)) > -2) {
		if (p == -1) {
			dump_page_freelist(page);
		} else if (p == 0) {
			for(int t=0; t < OM_OPP; ++t)
				if(t && t%8==0) printf("  "); else printf(" ");
			printf("|\n");
		} else {
			dump_page(page);
		}
		++page;
	}
	printf("=================== %d pages ======================", page);
	fflush(stdout);
	
	// dump the free page list
	printf("\nFree Page List\n");
	PageObjRef *top = g_free_page;
	while (top != g_free_page_root) {
		if ( (top - OM.r)%(OM_OPP*2) == 1) {
			printf("[%d]\n", *top + 1);
			top = (PageObjRef*)&OM.p[*top];
		} else {
			printf("%d ", *top + 2);
		}
		--top;
	}
	printf("[%d]\n", *top + 2);
	fflush(stdout);
	
	// dump the object lists
	printf("\nObjects Free List\n");
	int cnt = 0;
	FreeObjRef fo = OM.f16[3*1].next;
	while(fo != 3) {
		++cnt;
		fo = OM.f8[fo].next;
	}
	printf(" 3[  1]: %d\n",cnt);
	
	for (int s = 4; s < PAGE_SIZE_BITS; ++s) {
		printf("%2d[%3d]: ",s, (1<<s)/8);
		fflush(stdout);
		fo = OM.f16[s*1].next;
		cnt = 0;
		while (fo != s) {
			++cnt;
			fo = OM.f16[fo].next;
		}
		
		printf("%d\n",cnt);
	}
	fflush(stdout);
	
}


void test_alloc(void)
{
	INFO("%d bytes / tiny", sizeof(TinyObj));
	INFO("%d bytes / page,  %d objs/page", sizeof(PageObj), OM_OPP);
	
	
	omem_init(2);
	PageObjRef x;
	int nobj = 0;
	while ( (x=omem_alloc_page())) {
		INFO("%d", x);
		++nobj;
	}
	INFO("%d objs allocated", nobj);
	omem_fini();
}


void test_rand_alloc_pages(void)
{
	omem_init(1024*1024);
	srand(time(0));
	PageObjRef objs[10021] = {0};
	int nallocs = 0, nfrees=0, z;
	for (int i=0; i < 10021; ++i)
		objs[i] = omem_alloc_page();
	for (int i=0; i < 200000; ++i) {
		z = 0;
		//~ for (int q=0; q<1024; ++q)
			z += rand()%10021;
		//~ z>>=10;
		//~ if ((i+1)%100000 == 0) {
			//~ ORef o = 0;
			//~ int p;
			//~ dump_free_pages();
			

			//~ while ((p = omem_page_in_use(o)) > -2) {
				//~ printf("%c", p==-1?'O':(p==0?'.':'X' ));
				//~ o += OM_OPP;
			//~ }
			//~ printf("\n %d allocs,  %d free\n", nallocs, nfrees);
			//~ return;
		//~ }
		
		if (objs[z]) {
			//~ INFO("Free %x", objs[z]);
			omem_free_page(objs[z]);
			objs[z] = 0;
			++nfrees;
		} else {
			if (z*101 % 3 ==0)
			objs[z] = omem_alloc_page();
			//~ INFO("Alloc %x", objs[z]);
			++nallocs;
		}
	}
	
	omem_fini();
}

void alloc_smalls(void)
{
	omem_init(1024);
	ObjClass kls1 = {3, 0};
	ObjClass kls2 = {4, 0};
	omem_register_class(10, &kls1);
	omem_register_class(11, &kls2);
	ORef x = omem_alloc(10);
	INFO("got %x", x);
	ORef y = omem_alloc(11);
	INFO("got %x", y);
	
	ORef z = omem_alloc(10);
	INFO("got %x", z);
	
	
	omem_free(x);
	omem_free(y);
	omem_free(z);
	omem_alloc(10);
	dump_pages();
	
	omem_fini();
}

void alloc_random(void)
{
	#define NRANDOBJS 1024*1024
	ORef objects[NRANDOBJS] = {0};
	omem_init(1024*1024);
	srand(time(0));
	
	// intialize object types
	ObjClass kls = {0};
	int breaks[] = {0, 0, 0, 40, 60, 70, 78, 82, 88, 95, 97,100};
	
	uint8_t rand_type(void) {
		int x = rand()%100;
		int size = 3;
		while(size < 12 && x >= breaks[size])
			++size;
		return size;
	}
	
	for (int size=3; size <= 12; ++size) {
		kls.size = size;
		omem_register_class(size, &kls);
	}
	
	for(int turn=0; turn < 1000000; ++turn) {
		// pick one of our objects 
		int oi = rand()%NRANDOBJS;
		if (objects[oi]) {
			omem_free(objects[oi]);
			objects[oi] = 0;
		} else {
			//~ if (rand()%4)
			XINFO("-------");
				objects[oi] = omem_alloc(rand_type());
		}
	}
	
	ORef a = omem_alloc(12);
	ORef b = omem_alloc(12);
	ORef c = omem_alloc(12);
	ORef d = omem_alloc(12);
	omem_free(c);
	omem_free(b);
	omem_free(a);
	omem_free(d);
	
	dump_pages();
	
	
	
	omem_fini();
}

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
	//~ #~ test_rand_alloc_free();
	//~ alloc_smalls();
	alloc_random();
	//~ test_names();
	//~ test_parents();

	INFO("All tests passed!");
	return 0;
}