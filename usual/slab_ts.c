#include <usual/slab_ts.h>
#include <usual/slab_internal.h>
#include <usual/spinlock.h>

/*
 * Thread-safe wrapper for the primitive slab allocator.
 */
struct ThreadSafeSlab {
    struct List head;
    struct Slab *slab;
    SpinLock lock;
};

static void ts_slab_list_append(struct ThreadSafeSlab *ts_slab)
{
	statlist_append(&thread_safe_slab_list, &ts_slab->head);
}

static void ts_slab_list_remove(struct ThreadSafeSlab *ts_slab)
{
	statlist_remove(&thread_safe_slab_list, &ts_slab->head);
}

static void init_thread_safe_slab_and_store_in_list(struct ThreadSafeSlab *ts_slab, const char *name, unsigned obj_size,
		      unsigned align, slab_init_fn init_func,
		      CxMem *cx)
{
	init_slab(ts_slab->slab, name, obj_size, align, init_func, cx);
	ts_slab_list_append(ts_slab);
}

/*
 * Create a new thread-safe slab allocator.
 */
struct ThreadSafeSlab *thread_safe_slab_create(const char *name, unsigned obj_size, unsigned align,
                                               slab_init_fn init_func, CxMem *cx) {
    struct ThreadSafeSlab *ts_slab;

    ts_slab = cx ? cx_alloc0(cx, sizeof(*ts_slab)) : calloc(1, sizeof(*ts_slab));
    if (!ts_slab)
        return NULL;

    ts_slab->slab = cx_alloc0(cx, sizeof(*(ts_slab->slab)));

    if (!ts_slab->slab) {
        free(ts_slab);
        return NULL;
    }

    list_init(&ts_slab->head);
	init_thread_safe_slab_and_store_in_list(ts_slab, name, obj_size, align, init_func, cx);
    spin_lock_init(&ts_slab->lock);
    return ts_slab;
}

/* free all storage associated by thread-safe slab */
void thread_safe_slab_destroy(struct ThreadSafeSlab *ts_slab)
{
	if (!ts_slab)
		return;

	ts_slab_list_remove(ts_slab);
	slab_destroy_internal(ts_slab->slab);
	cx_free(ts_slab->slab->cx, ts_slab);
}

/*
 * Allocate one object from the slab.
 */
void *thread_safe_slab_alloc(struct ThreadSafeSlab *ts_slab) {
    void *obj;
    spin_lock_acquire(&ts_slab->lock);
    obj = slab_alloc(ts_slab->slab);
    spin_lock_release(&ts_slab->lock);
    return obj;
}

/*
 * Return object back to the slab.
 */
void thread_safe_slab_free(struct ThreadSafeSlab *ts_slab, void *obj) {
    spin_lock_acquire(&ts_slab->lock);
    slab_free(ts_slab->slab, obj);
    spin_lock_release(&ts_slab->lock);
}

/*
 * Get total number of objects allocated (capacity), including free and in-use.
 */
int thread_safe_slab_total_count(struct ThreadSafeSlab *ts_slab) {
    int count;
    spin_lock_acquire(&ts_slab->lock);
    count = slab_total_count(ts_slab->slab);
    spin_lock_release(&ts_slab->lock);
    return count;
}

/*
 * Get number of free objects in the slab.
 */
int thread_safe_slab_free_count(struct ThreadSafeSlab *ts_slab) {
    int count;
    spin_lock_acquire(&ts_slab->lock);
    count = slab_free_count(ts_slab->slab);
    spin_lock_release(&ts_slab->lock);
    return count;
}

/*
 * Get number of currently active (in-use) objects.
 */
int thread_safe_slab_active_count(struct ThreadSafeSlab *ts_slab) {
    int count;
    spin_lock_acquire(&ts_slab->lock);
    count = slab_active_count(ts_slab->slab);
    spin_lock_release(&ts_slab->lock);
    return count;
}

/*
 * Report stats for all slabs (global, not per instance).
 */
void thread_safe_slab_stats(slab_stat_fn cb_func, void *cb_arg) {
    struct ThreadSafeSlab *ts_slab;
	struct List *item;

	statlist_for_each(item, &thread_safe_slab_list) {
		ts_slab = container_of(item, struct ThreadSafeSlab, head);
        spin_lock_acquire(&ts_slab->lock);
		run_slab_stats(ts_slab->slab, cb_func, cb_arg);
        spin_lock_release(&ts_slab->lock);
	}
}
