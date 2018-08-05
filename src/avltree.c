#ifndef AVLTREE_C
#define AVLTREE_C

/** \file avltree.c

This provides a simple AVL Tree implementation for 62bit keys to user defined data.  There is no delete.

Memory is handled by mmap.  
There is a \a max_objs sized linear array of AVLNode objects allocated for use.
Objects are only allocated and never freed.  
The size of each AVLNode object (\a node_size ) changes with the size of the user data.
The first AVLNode is not a node, it stores the AVLTree data which is aliased on top.
The \a AVLTree.sub0 lines up on top of AVLNode.sub[0] so that it can act as a sentinal node,
and doubly it can hold usefull information about the tree, because only the sub[0] data member is needed.
The rest of the tree hangs off of the left branch sub[0] of the 'dummy' node AVLTree.  

User data must be at least 8 bytes and be a multiple of 8 bytes so that the uint64_t key will align propery.

*/

#include <stdint.h>

#define AVLP(self, AVLR) ((AVLNode*)( (char*)(self) + (self)->node_size * (AVLR)))
#define AVLR(self, ptr) ((uint32_t) ( ((char*)(ptr) - (char*)(self)) / (self)->node_size))

typedef struct s_AVLTree AVLTree;
typedef struct s_AVLNode AVLNode;

struct s_AVLTree {
	uint32_t sub0; // This aliases with sub[0] when children rotate under this root
	uint32_t node_size;  //size of a node (including user data)
	uint32_t depth; // the maximum depth of this tree (used to pre-allocate a history when inserting)
	uint32_t max_objs;  // The maximum number of objects that can be added to the tree
	uint32_t num_objs; // The current number of objects in this tree
};

struct s_AVLNode {
	uint32_t sub[2];
	uint64_t key:62;
	uint64_t balance:2; // 0:unused,  1: Left-heavy, 2:balanced, 3:right heavy
	char data[]; // user data must be at least 8 bytes and be a multiple of 8 bytes
};

/** Create a new tree.  
	\a user_data_size is sizeof(YourStruct).  It must be at least 8 bytes and be a multiple of 8.
	\a max_objs.  The maximum number of objects you will ever need to store. (pre-allocated virtual memory).
*/
AVLTree *avl_new(int user_data_size, int max_objs);

/** Free the tree. munmap
*/
void avl_fini(AVLTree *self);

/** Return a pointer to the user data associated with key, or NULL if key is not present.
*/
void* avl_find(AVLTree *self, uint64_t key);

/** Return a pointer to the user data associated with key, 
If key already exists in the tree then a pointer to the existing user data is returned.
If the key does not exist then a new user object is zeroed and returned.
*/
void* avl_insert(AVLTree *self, uint64_t key);

#if __INCLUDE_LEVEL__ == 0

#include <alloca.h>
#include <sys/mman.h>
#include <string.h>
#include "logging.c"

AVLTree *avl_new(int user_data_size, int max_objs)
{
	int node_size = user_data_size + sizeof(AVLNode);
	//~ INFO("node size %d bytes", node_size);
	ASSERT(node_size % 8 == 0, "Node data must be a multiple of 8 bytes (%d)", node_size);
	AVLTree *self = mmap(0, node_size * (max_objs+1), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, 0, 0);
	ASSERT(self != MAP_FAILED, "Can't mmap for AVL tree: ");
	
	self->max_objs = max_objs;
	self->num_objs = 0;
	self->node_size = node_size;
	self->depth = 1;
	self->sub0 = 0;
	return self;
}

void avl_fini(AVLTree *self)
{
	ASSERT(munmap(self, self->node_size * (self->max_objs+1)) == 0, "Failed to unmap AVL Tree");
}

void* avl_insert(AVLTree *self, uint64_t key)
{
	ASSERT(key ==  key & ~((uint64_t)3<<62), "Key truncated to 62 bits!");
	AVLNode **path = alloca(sizeof(AVLNode*) * (self->depth+2));
	char *dir = alloca(self->depth+2);
	dir[0] = 0;
	path[0] = (AVLNode*)self;  //node0 will alias with sub[0]
	
	// track to the end leaving a trail of breadcrumbs in path and dir
	AVLNode *node = AVLP(self, self->sub0), *child, *subchild;
	int depth = 0;
	while(node != (AVLNode*)self) {
		path[++depth] = node;
		if (node->key == key)
			return (void*)&node->data; // We found a node with this key
		dir[depth] = (key > node->key);
		node = AVLP(self, node->sub[dir[depth]]);
	}
	
	// create a new node at the end
	int new_depth = depth+1;
	if (new_depth > self->depth)
		self->depth = new_depth;
	node = path[depth];
	
	if (self->num_objs == self->max_objs)
		return NULL; // no more room for new nodes
	self->num_objs ++;
	child = AVLP(self, self->num_objs);
	memset(child, 0, self->node_size);
	child->key = key;
	child->balance = 2;	
	path[depth+1] = child;
	node->sub[dir[depth]] = AVLR(self, child);

	// backtrace and rebalance tree
	for (int balance, d; depth >= 1; child=node, --depth, node = path[depth]) {
		balance = node->balance + (dir[depth]? 1: -1);
		d = 1;
		switch((balance << 4) | child->balance) {
			// single rotations
			case 0x01:
				d = 0;
			case 0x43:
				node->sub[d] = child->sub[!d];
				child->sub[!d] = AVLR(self, node);
				node->balance = 2;
				child->balance = 2;
				path[depth-1]->sub[dir[depth-1]] = AVLR(self, child);
				node = child;
				break;
			// double rotations
			case 0x03:
				d=0;
			case 0x41:
				subchild = path[depth+2];
				node->sub[d] = subchild->sub[!d];
				child->sub[!d] = subchild->sub[d];
				subchild->sub[!d] = AVLR(self, node);
				subchild->sub[d] = AVLR(self, child);
				path[depth+(!d)]->balance = (subchild->balance == 3)? 1: 2;
				path[depth+d]->balance = (subchild->balance == 1)? 3: 2;
				subchild->balance = 2;
				path[depth-1]->sub[dir[depth-1]] = AVLR(self, subchild);
				node = subchild;
				break;
			default:
				node->balance = balance;
				break;
		}
		
		if (node->balance == 2)
			break; // We absorbed the insertion imbalance here.
	}
	
	return (void*)&(path[new_depth]->data);
}

void* avl_find(AVLTree *self, uint64_t key)
{
	AVLNode *node = AVLP(self, self->sub0);
	while (node !=(AVLNode*)self) {
		if (node->key == key)
			return (void*)&node->data;
		node = AVLP(self, node->sub[key > node->key]);
	}
	return NULL;
}

#endif
#endif
