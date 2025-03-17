#include <usual/slab_ts.h>
#include <usual/slab_internal.h>
#include <usual/spinlock.h>

/*
 * Thread-safe wrapper for the primitive slab allocator.
 */
struct ThreadSafeSlab {
    struct Slab *slab;
    SpinLock lock;
};

/*
 * Create a new thread-safe slab allocator.
 */
struct ThreadSafeSlab *thread_safe_slab_create(const char *name, unsigned obj_size, unsigned align,
                                               slab_init_fn init_func, CxMem *cx) {
    struct ThreadSafeSlab *ts_slab;

    ts_slab = cx ? cx_alloc0(cx, sizeof(*ts_slab)) : calloc(1, sizeof(*ts_slab));
    if (!ts_slab)
        return NULL;

    ts_slab->slab = slab_create_internal(name, obj_size, align, init_func, cx, true);
    if (!ts_slab->slab) {
        free(ts_slab);
        return NULL;
    }

    spin_lock_init(&ts_slab->lock);
    return ts_slab;
}

/*
 * Destroy a thread-safe slab allocator and free all associated memory.
 */
void thread_safe_slab_destroy(struct ThreadSafeSlab *ts_slab) {
    if (!ts_slab)
        return;
    spin_lock_acquire(&ts_slab->lock);
    slab_destroy_internal(ts_slab->slab, true);
    spin_lock_release(&ts_slab->lock);
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
    slab_stats(cb_func, cb_arg);
}
