#include <stdlib.h>
#include <time.h>
#include <stdio.h>
#include "avltree.c"
#include "logging.c"

typedef const char* LabelFunc(uint64_t key, void *data);

void avl_stats(AVLTree *self, const char *filename, LabelFunc* func)
{
#define FOUT(fmt, ...)   do{snprintf(buffer, 128, fmt, ## __VA_ARGS__); fputs(buffer, file);}while(0)
	int64_t null_counter =0, subkey;
	char buffer[128];
	
	if (!filename)
		return;
	FILE *file = fopen(filename, "w");
	
	FOUT("digraph {\n");
	
	int64_t dump_node(uint32_t node_AVLR) {
		//~ INFO("Dump node %d\n", node_AVLR);
		AVLNode *node = AVLP(self, node_AVLR);
		if (node_AVLR == 0) {
			FOUT("n%u [shape=circle, label=\"\", color=black];\n",  (uint32_t)(-(--null_counter)));
			return null_counter;
		}
		FOUT("p%u [shape=circle, label=\"",(uint32_t)node->key);
		if (func)
			FOUT("%s\", color=", func(node->key, node->data));
		else
			FOUT("%u\", color=", (uint32_t)node->key);
		
		switch(node->balance) {
			case 1:FOUT("red];\n");break;
			case 2:FOUT("black];\n");break;
			case 3:FOUT("blue];\n");break;
			default:FOUT("green];\n");break;
		}
		
		subkey = dump_node(node->sub[0]);
		FOUT("\tp%u -> %c%u [color=red];\n", (uint32_t) node->key, subkey<0 ? 'n' : 'p', (uint32_t)(subkey<0 ? -subkey : subkey));
		subkey = dump_node(node->sub[1]);
		FOUT("\tp%u -> %c%u [color=black];\n", (uint32_t)node->key, subkey<0 ? 'n' : 'p', (uint32_t)(subkey<0 ? -subkey : subkey));
		return node->key;
	}
	dump_node(self->sub0);
	FOUT("}\n");
	fclose(file);
}

void test_small_tree(void)
{
	AVLTree *tree = avl_new(8, 26);
	for (char c = 'A'; c <= 'Z'; ++c) {
		char *data = avl_insert(tree, c+1000);
		ASSERT(data, "Why are we null?");
		*data = c;
	}
	ASSERT(avl_insert(tree, 1) == 0, "There should be no more space!");
	
	for (char c = 'A'; c < 'Z'; ++c) {
		char *data = avl_find(tree, c+1000);
		ASSERT(*data == c, "Bad recall");
	}
	ASSERT(tree->num_objs == 26, "Not 26 internally! %d", tree->num_objs);
	
}


void test_duplicates(void)
{
	AVLTree *tree = avl_new(8, 100);
	
	for (int i=0; i < 10000; ++i) {
		avl_insert(tree, (i*23) % 100);
	}
	
	int total = 0;
	void check_node(uint32_t node_AVLR) {
		if (!node_AVLR)
			return;
		AVLNode *node = AVLP(tree, node_AVLR);
		total ++;
		check_node(node->sub[0]);
		check_node(node->sub[1]);
	}
	check_node(tree->sub0);
	ASSERT(total == 100, "Not 100! %d", total);
	ASSERT(tree->num_objs == 100, "Not 100 internally! %d", tree->num_objs);
}

void test_random_big_tree(void)
{
	AVLTree *tree = avl_new(8, 1<<20);
	srand(time(0));
	for (int i=0; i < (1<<20)-1; ++i)
		avl_insert(tree, rand());
	int total = 0;
	uint64_t check_node(uint32_t node_AVLR, int depth) {
		if (!node_AVLR)
			return ((uint64_t)depth<<32) | depth;
		total ++;
		AVLNode *node = AVLP(tree, node_AVLR);
		if (node->sub[0] && AVLP(tree, node->sub[0])->key > node->key)
			ABORT("Out of order");
		if (node->sub[1] && AVLP(tree, node->sub[1])->key < node->key)
			ABORT("Out of order");
		uint64_t d1 = check_node(node->sub[0], depth+1);
		uint64_t d2 = check_node(node->sub[1], depth+1);
		int diff = (int)(d1 >> 32) - (int)(d2>>32);
		if (diff < -1 || diff > 1)
			ABORT("Not AVL %d", diff);
		uint32_t max = (d1>>32) > (d2>>32) ? (d1>>32) : (d2>>32);
		uint32_t min = (d1&0xFFFFFFFF) < (d2&0xFFFFFFFF) ? (d1&0xFFFFFFFF) :(d2&0xFFFFFFFF);
		return ((uint64_t)max<<32) | min;
	}
	
	uint64_t depth = check_node(tree->sub0, 0);
	
	INFO("Total = %d, Depth = %d ~ %d", total, depth&0xFFFFFFFF, depth>>32);
	ASSERT(tree->num_objs == total, "Not %d internally! %d", total, tree->num_objs);
	
}


int main(int argc, char *argv[])
{
	test_small_tree();
	test_random_big_tree();
	test_duplicates();
	INFO("Good Tests!");
	return 0;
}


