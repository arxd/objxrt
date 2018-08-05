#ifndef INTSTR_C
#define INTSTR_C

#include <stdint.h>
#include "avltree.c"

#define MAX_STRINGS ((uint32_t)16*1024*1024)
#define MAX_STRING_DATA ((uint32_t)1*1024*1024*1024)

typedef uint32_t IStr;

void is_init(void);
void is_fini(void);

IStr is_get(const char *utf8);
const char* is_utf8(IStr string_atom);
void is_stats(const char *filename);
extern uint32_t is_hash_collisions; // number of hash collisions
extern AVLTree *is_tree;
IStr is_next(IStr intstr);

#if __INCLUDE_LEVEL__ == 0

#include <string.h>
#include <stdio.h>
#include "logging.c"
#include <sys/mman.h>

static char *g_string_data;  // length of MAX_STRING_DATA
static char *g_string_data_tail; // start of free space
AVLTree *is_tree; // mapping of IStr to pointers to the data
uint32_t is_hash_collisions; // statistics

static uint32_t hash_utf8(const char *utf8)
{
	uint32_t hash = 2147487509;
	const char *c = utf8;
	while (*c) {
		hash *= 11;
		hash = hash << 7 | hash >> 32-7;
		hash += (*c);
		++c;
	}
	return hash?hash:1;
}

void is_init(void)
{
	is_tree = avl_new(8, MAX_STRINGS);
	g_string_data = mmap(0, MAX_STRING_DATA, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
	ASSERT(g_string_data != MAP_FAILED, "Can't mmap for intstr ");
	g_string_data_tail = g_string_data;
	is_hash_collisions = 0;
}

void is_fini(void)
{
	avl_fini(is_tree);
	ASSERT(munmap(g_string_data, MAX_STRING_DATA) == 0, "Failed to unmap AVL Tree");
}

static IStr _is_get(const char *utf8, uint32_t hash)
{
	// find out what is at 'hash'
	char **str = (char**)avl_insert(is_tree, hash);
	if (!str)
		return 0; // no more space
	// is this hash location empty?
	if (!*str) {
		int len = strlen(utf8) + 1;
		if (g_string_data_tail + len >= g_string_data+MAX_STRING_DATA)
			return 0; // no more space
		*str = g_string_data_tail;
		g_string_data_tail += len;
		memcpy(*str, utf8, len);
		return hash;
	}
	
	// make sure this is our string
	int diff;
	const char *a = *str, *b = utf8;
	while (0 == (diff = (*a - *b))) {
		if (*a == 0 && *b == 0)
			return hash; // this is our string;
		++a;
		++b;
	}
	
	// hash collision.  move hash to a new location and try again.
	++is_hash_collisions;
	//~ INFO("Hash Collision |%s|%x  [%d]  |%s|%x", *str, hash, diff, utf8, hash +  diff * 123456791);
	hash +=  ((uint8_t)diff) * 123456791;
	return _is_get(utf8, hash?hash:1);
}

IStr is_get(const char *utf8)
{
	return _is_get(utf8, hash_utf8(utf8));
}

const char* is_utf8(IStr intstr)
{
	char **str = (char**)avl_find(is_tree, intstr);
	return str ? *str : NULL;
}

IStr is_next(IStr intstr)
{
	IStr ref_best = 0, ref = is_tree->sub0;
	while(ref) {
		AVLNode *node = AVLP(is_tree, ref);
		if (intstr >= node->key) {
			ref = node->sub[1];
		} else {
			ref_best = node->key;
			ref = node->sub[0];
		}
	}
	return ref_best;
}

#endif // code
#endif // header
