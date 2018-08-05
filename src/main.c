#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include "logging.c"
#include "blockmem.c"
#include "avltree.c"
#include "intstr.c"
#include "objid.c"
#include "links.c"
#include "objmem.c"

void read_words(int argc, char *argv[])
{
	char line[256];
	FILE *f  = fopen(argv[1],"r");
	int c, i=0;
	while ((c=fgetc(f)) != EOF) { 
		//~ INFO("%c<%d", c,i);
		if (isalpha(c))
			line[i++] = c;
		else if (i) {
			line[i++] = 0;
			is_get(line);
			//~ INFO("%s", line);
			i = 0;
		}
	}
	fclose(f);
	
	for (int i =0; i < 500000; ++i) {
		const char * s = is_utf8(i);
		if (s)
			INFO("%u | %s", i, s);
	}
	
	
}


//~ MMList g_list;

int main(int argc, char *argv[])
{
	//~ is_init(100000, 8);
	objm_init();
	while(1);
	//~ links_init();
	//~ for(int i=0; i < 50; ++i)
		//~ links_add(rand()%10000, 7, LINK_DATA);
	//~ links_stats();
	//~ for(int i=0; i < 3; ++i) {
		//~ links_add(101, 100, LINK_DATA);
		//~ links_add(101, 100, LINK_NAME);
		//~ links_add(101, 100, LINK_PARENT);
		//~ links_add(101, 100, LINK_CHILD);
		//~ links_add(101, 100, LINK_PROTO);
		//~ links_add(101, 100, LINK_OTHER);
		
	//~ }
	//~ for(int i=0; i<10; ++i) {
		//~ links_add(90, 110, LINK_DATA);
	//~ }
	//~ ID links[100];
	//~ int got = links_get_list(101, LINK_ALL, 13, links);
	//~ INFO("got %d:", got);
	//~ for (int i=0; i < got; ++i) 
		//~ INFO("%d: %d", links[i], links[i]&0x7);
	
	//~ void callback(ID b, LinkType type) {
		//~ INFO("got %u : %d", b, type);
	//~ }
	
	//~ links_each(101,LINK_CHILD, callback);
	
	//~ links_debug("links.dot");
	//~ is_fini();
	//~ while(1);
	return 0;
}