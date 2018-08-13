/**
A generic object can do everything.
  - any number of key/value
  - any number of array elements (max 16million)
  - any number of parents
  - user data ptr


*/

typedef struct s_GenericObject GenericObject;

struct s_GenericObject {
	uint32_t type:8;
	uint32_t data_high:24;
	uint32_t data_low;
	uint32_t length;
	ORef keys;
	ORef vals;
	ORef protos[2];
	ORef next;
};


ORef list_get(ORef obj, uint32_t idx) {
	int len;
	ORef *refs;
	while(1) {
		len = (1<<(OM.f8[obj].size-2)) - 2;
		refs = &OM.f8[obj].next;
		if (idx < len)
			break;
		idx -= len;
		obj = refs[len];
	}
	return refs[idx];
}

ORef generic_obj_get_idx(ORef obj, uint32_t idx)
{
	GenericObject *gobj = &OM.o[obj];
	return omem_reflist_get(gobj->list, idx);
}

ORef generic_obj_set_idx(ORef obj, int idx, ORef val)
{
	GenericObject *gobj = &OM.o[obj];
	omem_reflist_set(gobj->list, idx, val);
	return val;
}

ORef generic_obj_ins_idx(ORef obj, int idx, ORef val)
{
	GenericObject *gobj = &OM.o[obj];
	omem_reflist_ins(gobj->list, idx, val);
	return val;
}

ORef generic_obj_get_istr(ORef obj, IStr key_istr)
{
	GenericObject *gobj = &OM.o[obj];
	return omem_kwlist_get(gobj->dict, key_istr);
}

ORef generic_obj_set_istr(ORef obj, IStr key_istr, ORef val)
{
	GenericObject *gobj = &OM.o[obj];
	omem_kwlist_set(gobj->dict, key_istr, val);
	return val;
}

void *generic_obj_get_data(ORef obj)
{
	GenericObject *gobj = &OM.o[obj];
	return (void*)(((uint64_t)gobj->data_high<<32) | gobj->data_low);
}

void generic_obj_set_data(ORef obj, uint64_t data_ptr)
{
	GenericObject *gobj = &OM.o[obj];
	gobj->data_high = data_ptr>>32;
	gobj->data_low = data_ptr & 0xFFFFFFFF;
}

uint32_t obj_get_length(ORef obj)
{
	GenericObject *gobj = &OM.o[obj];
	return gobj->length;
}

void generic_obj_free(ORef obj)
{
	
}

ObjClass generic_object_class = {5,  };




