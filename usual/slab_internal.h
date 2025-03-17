#ifndef _USUAL_SLAB_INTERNAL_H_
#define _USUAL_SLAB_INTERNAL_H_

#include <usual/statlist.h>
#include <usual/slab.h>
#include <usual/spinlock.h>

/*
* Store for pre-initialized objects of one type.
*/
struct Slab {
	struct List head;
	struct StatList freelist;
	struct StatList fraglist;
	char name[32];
	unsigned final_size;
	unsigned total_count;
	slab_init_fn  init_func;
	CxMem *cx;
};

/* keep track of all active slabs */
static STATLIST(slab_list);
static STATLIST(thread_safe_slab_list);

void slab_list_append(struct Slab *slab, bool is_thread_safe);
void slab_list_remove(struct Slab *slab, bool is_thread_safe);

struct Slab *slab_create_internal(const char *name, unsigned obj_size, unsigned align,
                                  slab_init_fn init_func, CxMem *cx, bool is_thread_safe);

void slab_destroy_internal(struct Slab *slab, bool is_thread_safe);

#endif /* _USUAL_SLAB_INTERNAL_H_ */
