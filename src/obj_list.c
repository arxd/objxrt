/**
This is a medium sized list.
It can hold up to 64k items
80% space efficient

type 

*/

#ifndef LISTM_C
#define LISTM_C

#include "obj.c"

#define LISTM_SIZE 6
#define LISTM_NELS 13
#define OM_LIST(id) ((ObjListM*)&OM.f8[id])
//~ #define OM_LISTPATH(path) ((ObjListM*)&OM.f8[path.p[path.i]])
#define PATH_DIR (path->d[path->i])
#define PATH_OBJ (path->p[path->i])
#define PATH_OBJX(x)  (path->p[path->i + (x)])
#define PATH_SELF ((ObjListM*)&OM.f8[PATH_OBJ])

typedef struct s_ObjListM ObjListM;
typedef struct s_Path Path;


struct s_Path {
	ORef p[16];
	char d[16];
	int i;
	int idx;
};

struct s_ObjListM {
	uint32_t type:8; // 64b class
	uint32_t off:4;
	uint32_t index:20;
	ORef child[2]; // left and right
	ORef els[LISTM_NELS];
};

ORef listm_new(void);

/**\breif Calculate the length of this list */
int listm_count(ORef list);

/**\brief Return the reference at indexed position \a idx*/
ORef listm_get_idx(ORef list, int idx);
/**\brief Set indexed position \a idx to \a val*/
ORef listm_set_idx(ORef list, int idx, ORef val);
/**\brief Insert \a val into list at \a idx position */
ORef listm_ins_idx(ORef *list, int idx, ORef val);
/**\brief Remove \a idx and return its value */
//~ ORef listm_pop_idx(ORef list, int idx);
//~ void listm_rem_slice(ORef list, int from, int to);
/**\brief Insert a slice of \a other into this list at position \a idx*/
//~ void listm_merge(ORef list, int idx, ORef other, int from, int to);


int set_get_idx(ORef set, ORef key);
int set_ins_key(ORef *list, ORef key);


ObjListM *list_iter_init(ORef list, Path *path, int start_idx);
ObjListM *list_iter_next(Path *path);



#if __INCLUDE_LEVEL__ == 0
/** ============== Helpful path stuff ==============
*/

#include <string.h>
#include "logging.c"

void dump_path(Path *path, const char *info)
{
	char buffer[256];
	int pos=0;
	for (int i=0; i <= path->i; ++i) {
		pos += snprintf(buffer+pos, 256-pos, "%c %d ", path->d[i]?'R':'L', OM_LIST(path->p[i])->index);
		if (pos < 0) ABORT("patherr");
		if (pos >= 256) ABORT("too long");
	}
	snprintf(buffer+pos, 256-pos, " (%d)", path->idx); 
	XXINFO("%s: %s",info, buffer);
	
}

void dump_listd(ORef list, const char *filename)
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
		
		FOUT("struct%u [label=<<table border=\"0\" cellborder=\"1\" cellspacing=\"0\"><tr><td", lob);
		
		if (self->type == TYPE_LIST_IMBAL)
			FOUT(" bgcolor=\"red\"");
		if (self->type == TYPE_LIST_END)
			FOUT(" bgcolor=\"green\"");
		FOUT(">0x%x</td><td>%d</td></tr><tr>", lob, self->index);
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

static ObjListM *path_next(ObjListM *self, Path *path)
{
	if (self->child[1] == 0) { // go back up and around the right
		while(PATH_DIR == 1)
			--path->i;
		--path->i;
	} else {
		//PATH_DIR = 1;// go right
		++ path->i;
		PATH_OBJ = self->child[1];
		PATH_DIR = 1; // and down left		
		while ((self = PATH_SELF)->child[0]) {
			++ path->i;
			PATH_DIR = 0;
			PATH_OBJ = self->child[0]; // grab all the left nodes
		}
	}
	return PATH_SELF;
}

static ObjListM *set_find_idx(Path *path, ORef key)
{
	ObjListM *self;
	while(1)  {
		self = PATH_SELF;
		if (self->type == TYPE_LIST_END) {
			// look for a good place to put key in the current block
			while (self->els[path->idx] && self->els[path->idx] < key)
				path->idx += 1;
			break;
		}
		if (key < self->els[self->off]) {
			++ path->i;
			PATH_DIR = 0;
			if (! (PATH_OBJ = self->child[0])) {
				// we want to go left but we can't.  Put it at zero here.
				path->idx = 0;//self->off;
				-- path->i;
				return self;
			}

		} else if (key > self->els[(self->off + LISTM_NELS-1)%LISTM_NELS]) {
			++ path->i;
			PATH_DIR = 1;
			if (! (PATH_OBJ = self->child[1])) {
				--path->i;
				return path_next(self, path);
				
				ObjListM *child = OM_LIST(PATH_OBJX(-2));
				//~ XXINFO("%d > %d > %d", 
					//~ child->els[child->off], key, self->els[(self->off + LISTM_NELS-1)%LISTM_NELS]);
				
				//~ XXINFO("Put %d in [%d] not [%d]", key, child->index, self->index);
				// we want to go right but we can't.  put it at zero in the previous element
				path->i -= 2;
				self = PATH_SELF;
				path->idx = 0;//self->off;
				ASSERT(child == self, "HUH");
				//~ ASSERT(path->idx == child->off, "bad off");
				//~ if (PATH_SELF->index < OM_LIST(PATH_OBJX(1))->index) {
					//~ XXINFO("DUMP");
					//~ dump_listd(path->p[0], "error.dot");
	
				//~ }
				//~ DBG_ASSERT( path->i >= 0, "No previous parent");
				//~ DBG_ASSERT( PATH_SELF->index == OM_LIST(PATH_OBJX(1))->index + 1, "Out of order %d, %d", PATH_SELF->index, OM_LIST(PATH_OBJX(1))->index);
				return self;
			}

		} else { // put it somewhere in this block
			path->idx = 0;
			while (self->els[(path->idx +self->off)%LISTM_NELS] < key)
				path->idx += 1;
			break;
		}
	}
	DBG_ASSERT( path->idx < LISTM_NELS, "Didn't fit in this block! %d", path->idx);
	return self;
}

static ObjListM *list_find_idx(Path *path, int page_idx)
{
	ObjListM *self;
	while((self = PATH_SELF)->index != page_idx) {
		++ path->i;
		PATH_DIR = page_idx > self->index;
		PATH_OBJ = self->child[PATH_DIR];
	}
	return self;
}

static ORef roll_node(ObjListM *self, Path *path, ORef val)
{
	dump_path(path, "ROLL: ");

	int i = self->off = (self->off + LISTM_NELS - 1 )%LISTM_NELS; // roll left
	ORef tmp = self->els[i]; // grab the last element
	while (path->idx) {
		int j = (i+1)%LISTM_NELS;
		self->els[i] = self->els[j];
		--path->idx;
		i = j;
	}
	self->els[i] = val;
	return tmp;
}



static int insert_into_end(ObjListM *self, Path *path, ORef val)
{
	// shift the elements right until we get to path->idx
	// for the end node self->off is the length of the list
	for (int j = self->off; j > path->idx; --j)
		self->els[j] = self->els[j-1];
	self->els[path->idx] = val;
	return (++self->off) == LISTM_NELS;
}

static ObjListM *create_new_block(ObjListM *self, Path *path)
{
	ObjListM *child;
	ASSERT(self->index < 0xFFFFFF, "Out of memory for list");
	
	self->off = 0; // off is now an offset
	self->type = TYPE_LIST_IMBAL; // we are a normal node now
	
	// add a new object to the path
	++ path->i;
	self->child[1] = PATH_OBJ = omem_alloc(LISTM_SIZE);
	
	// init the memory
	child = PATH_SELF;
	memset(child, 0, 1<<LISTM_SIZE);
	child->type = TYPE_LIST_END;
	child->index = self->index + 1;
	return child;
}

static ORef rebalance_tree(ObjListM *child, Path *path) {
	ObjListM *self;
	while (--path->i >= 0) {
		self = PATH_SELF;
		XXINFO("check balance [%d] %d", self->index, self->type);
		if (self->type == TYPE_LIST_IMBAL) {
			if (child->type == TYPE_LIST_IMBAL) {
				// rebalance here
				XXINFO("Rebalace [%d] [%d]", self->index, child->index);
				self->child[1] = child->child[0];
				child->child[0] = PATH_OBJ;
				PATH_OBJ = PATH_OBJX(1);
				child->type = self->type = TYPE_LIST_BAL;
				if (path->i > 0) // the root node has no parent, so be careful
					OM_LIST(PATH_OBJX(-1))->child[1] = PATH_OBJX(1);
				break; // we are all balanced now!
			}
		} else {
			self->type = TYPE_LIST_IMBAL;
		}
		child = self;
	}
	return path->p[0];
}

static ORef list_insert(ObjListM *self, Path *path, ORef val)
{
	// roll the objects until the end
	while ( self->type != TYPE_LIST_END) {
		val = roll_node(self, path, val);
		self = path_next(self, path);
	}
	// insert the value into the end node
	if (insert_into_end(self, path, val)) {
		// need to create a new block
		ObjListM * child = create_new_block(self, path);
		rebalance_tree(child, path);
	}
	return path->p[0];
}


ORef listm_new(void)
{
	ORef list = omem_alloc(LISTM_SIZE);
	ObjListM *self = OM_LIST(list);
	memset(self, 0, 1<<LISTM_SIZE);
	self->type = TYPE_LIST_END;
	return list;
}


int listm_count(ORef list)
{
	ObjListM *self;
	while((self = OM_LIST(list))->child[1])
		list = self->child[1];
	return self->index*LISTM_NELS + self->off;
}

static ObjListM *goto_idx(ORef *list, int idx)
{
	idx /= LISTM_NELS;
	ObjListM *self;
	while((self = OM_LIST(*list))->index != idx)
		*list = self->child[idx > self->index];
	return self;
}

ORef listm_get_idx(ORef list, int idx)
{	
	ObjListM *self = goto_idx(&list, idx);
	XXINFO("GET 0x%x, %d %d", list, idx, (idx + self->off)%LISTM_NELS);
	return self->els[(idx + self->off)%LISTM_NELS];
}

ORef listm_set_idx(ORef list, int idx, ORef val)
{
	ObjListM *self = goto_idx(&list, idx);
	self->els[(idx + self->off)%LISTM_NELS] = val;
	return val;
}

ORef listm_ins_idx(ORef *list, int idx, ORef val)
{
	Path path = {0};
	path.p[0] = *list;
	path.idx = idx % LISTM_NELS;
	ObjListM *self = list_find_idx(&path, idx / LISTM_NELS);

	*list = list_insert(self, &path, val);
	return val;
}


int set_get_idx(ORef set, ORef key)
{
	if (!key) // no NULL key's here
		return -1;
	ObjListM *self;
	while(1)  {
		self = OM_LIST(set);
		if (self->type == TYPE_LIST_END)
			break;
		if (key < self->els[self->off])
			set = self->child[0];
		else if (key > self->els[(self->off + LISTM_NELS - 1)%LISTM_NELS])
			set = self->child[1];
		else break;
	}
	// scan the current block for key
	int idx = 0;
	while (idx < LISTM_NELS && self->els[idx] != key)
		++idx;
	return idx==LISTM_NELS? -1: idx;
}

int set_ins_key(ORef *list, ORef key)
{
	ASSERT(key != 0, "No NULL keys allowed");
	Path path = {0};
	path.p[0] = *list;
	ObjListM *self = set_find_idx(&path, key);
	int index = (self->type == TYPE_LIST_END)?path.idx: (path.idx+self->off)%LISTM_NELS;
	if (self->els[index] == key)
		return self->index * LISTM_NELS + path.idx;
	XINFO("Insert %d @ %d", key, self->index);
	dump_path(&path, "start: ");
	*list = list_insert(self, &path, key);
	return index;
}


ObjListM *list_iter_init(ORef list, Path *path, int start_idx)
{
	memset(path, 0, sizeof(Path));
	path->p[0] = list;
	path->idx = start_idx % LISTM_NELS;
	ObjListM *self = list_find_idx(path, start_idx / LISTM_NELS);
	int tmp = path->i;
	path->i = (self->type == TYPE_LIST_END)? path->idx: (path->idx + self->off)%LISTM_NELS;
	path->idx = tmp;
	return (self->type == TYPE_LIST_END && path->i == self->off)? 0: self;
}

ObjListM *list_iter_next(Path *path)
{
	ObjListM *self = OM_LIST(path->p[path->idx]);
	if (self->type == TYPE_LIST_END) {
		path->i ++;
		if (path->i == self->off)
			return 0;
	} else {
		path->i = (path->i + 1) % LISTM_NELS;
		if (path->i == self->off) {
			path->i = path->idx;
			self = path_next(self, path);
			path->idx = path->i;
			if (self->type == TYPE_LIST_END) {
				if (self->off == 0)
					return 0;
				path->i = 0;
			} else {
				path->i = self->off;
			}
		}
	}
	return self;
}



#endif
#endif

