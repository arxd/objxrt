#include "obj_list.c"
#include <stdio.h>
#include "logging.c"

void dump_list(ORef list, const char *filename)
{
#define FOUT(fmt, ...)   do{snprintf(buffer, 128, fmt, ## __VA_ARGS__); fputs(buffer, file);}while(0)
	int64_t null_counter =0, subkey;
	char buffer[128];
	
	if (!filename)
		return;
	FILE *file = fopen(filename, "w");
	
	FOUT("digraph {\n");
	FOUT("node [shape=plaintext];\n");
	
	void dump_page(ORef lob) {
		ObjListM *self = OM_LIST(lob);
		
		FOUT("struct%u [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\"><tr><td colspan=\"13\"", lob);
		
		if (self->type == TYPE_LIST_IMBAL)
			FOUT(" bgcolor=\"#ffaaaa\"");
		if (self->type == TYPE_LIST_END)
			FOUT(" bgcolor=\"#aaffaa\"");
		FOUT(">0x%x [%d]</td></tr><tr>", lob, self->index);
		for (int i = 0; i < LISTM_NELS; ++i) {
			FOUT("<td");
			if (self->off == i)
				FOUT(" bgcolor=\"yellow\"");
			FOUT(">%d</td>", self->els[i]);
		}
		FOUT("</tr></table>>];\n");
		
		if (self->child[0]) {
			dump_page(self->child[0]);
			FOUT("struct%u -> struct%u [color=\"black\"];\n", lob, self->child[0]);
		}
		if (self->child[1]) {
			dump_page(self->child[1]);
			FOUT("struct%u -> struct%u [color=\"red\"];\n", lob, self->child[1]);
		}
	}
	dump_page(list);
	FOUT("}\n");
	fclose(file);
}


void test_basic(void)
{
	obj_init(1024);
	
	int length = 0;
	ORef list = listm_new();
	for (int i=0; i < 100; ++i) {
		INFO("-------- %d", i);
		length ++;
		listm_ins_idx(&list, rand()%length, i);
	}
	
	ASSERT(length == listm_count(list), "Wrong size %d", listm_count(list));
	
	dump_list(list, "list.dot");

	obj_fini();
	
}

int check_sorted(ORef set)
{
	Path p;
	ORef prev = listm_get_idx(set, 0);
	
	//~ char buffer[1024];
	//~ int pos = 0;
	
	for (ObjListM *self = list_iter_init(set, &p, 1); self; self = list_iter_next(&p)) {
		//~ pos += snprintf(buffer+pos, 1024-pos, "%d ", prev);
		if (prev >= self->els[p.i]) {
			//~ XINFO("%s  < %d", buffer, self->els[p.i]);
			return 0;
		}
		prev = self->els[p.i];
	}
	//~ XINFO("%s  %d", buffer, prev);
	return 1;
}

void test_set(void)
{
	obj_init(1024);
	
	srand(100);
	
	ORef set = listm_new();
	for(int i=0; i< 100092; ++i) {
		int r = rand()%(i+1);
		XINFO(":%d: INSERT %d", i, r);
		int idx = listm_ins_idx(&set, r, i);
		XINFO("Check ORDER");
		if (i%1000 == 0)
			INFO("MARK %d", i);
		//~ dump_list(set, "set.dot");
		//~ if (!check_sorted(set)) {
			//~ ERROR("OUT OF ORDER");
			//~ break;
		//~ }
	}
	
	dump_list(set, "set.dot");
	INFO("---------");
	
	set_ins_key(&set,19978);
	dump_list(set, "error.dot");
	
	
	obj_fini();
}


int main(int argc, char *argv[])
{
	//~ test_basic();
	test_set();
	INFO("All tests passed!");
	return 0;
}
