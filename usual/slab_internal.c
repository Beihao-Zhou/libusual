#include <usual/slab_internal.h>

void slab_list_append(struct Slab *slab, bool is_thread_safe)
{
	if (is_thread_safe) {
		statlist_append(&thread_safe_slab_list, &slab->head);
	} else {
#ifndef _REENTRANT
		statlist_append(&slab_list, &slab->head);
#endif
	}
}

void slab_list_remove(struct Slab *slab, bool is_thread_safe)
{
	if (is_thread_safe) {
		statlist_remove(&thread_safe_slab_list, &slab->head);
	} else {
#ifndef _REENTRANT
		statlist_remove(&slab_list, &slab->head);
#endif
	}
}
