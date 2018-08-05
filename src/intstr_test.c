#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "intstr.c"
#include "logging.c"

void test_simple(void)
{
	is_init();

	IStr The = is_get("The");	
	IStr the = is_get("the");	
	ASSERT(the != The, "bad");
	ASSERT(the == is_get("the"), "bad2");
	const char *theout = is_utf8(the);
	ASSERT(strcmp(theout, "the") == 0, "bad utf8");
	ASSERT(strcmp(is_utf8(The), "The") == 0, "bad utf8");
	ASSERT(is_utf8(23) == NULL, "why does this exist?");
	
	IStr x=0;
	while ( (x = is_next(x)) ) {
		printf("%s, ", is_utf8(x));
	}
	printf("\n");
	fflush(stdout);
	INFO("%d", is_hash_collisions);
	
	is_fini();
}

void test_book(const char *filename)
{
	uint32_t words = 0;
	uint32_t inserts = 0;
	char buffer[1024*800];
	is_init();
	getchar();
	
	FILE *f = fopen(filename, "r");
	ASSERT(f, "Can't open '%s' for reading.", filename);
	int c, i=0;
	while ( (c=fgetc(f)) != EOF) {
		if (isalpha(c) || isdigit(c) || c == '_') {
			buffer[i++] = c;
		} else {
			buffer[i] = 0;
			inserts++;
			is_get(buffer);
			i = 0;
		}
	}
	fclose(f);
	
	IStr x=0;
	
	uint64_t data = 0;
	while ( (x = is_next(x)) ) {
		const char *word = is_utf8(x);
		printf("%s, ", is_utf8(x));
		++words;
		data += strlen(word);
	}
	
	printf("\n");
	fflush(stdout);
	INFO("%d words, %d inserts, %d collisions",words, inserts, is_hash_collisions);
	INFO("%d characters, %.1f chars/word",  data, (double)data/(double)words);
	getchar();
	is_fini();
}

void test_a_million_numbers(uint32_t len)
{
	char buffer[25];
	is_init();
	getchar();
	
	for (int i=0; i < len; ++i) {
		snprintf(buffer,25,"%d",i);
		ASSERT(is_get(buffer), "out of space");
	}
	INFO("%d collisions", is_hash_collisions);
	
	IStr x=0;
	uint64_t data = 0, words=0;
	while ( (x = is_next(x)) ) {
		const char *word = is_utf8(x);
		++words;
		data += strlen(word);
	}
	INFO("%d words, %d chars,  %d kbytes", words, data, (data + 25*words)/1024);
	
	getchar();	
	is_fini();
}


int main(int argc, char *argv[])
{
	test_simple();
	test_book("intstr_test_data.txt");
	test_a_million_numbers(1<<20);
	
	INFO("All Tests good!");
	return 0;
}

