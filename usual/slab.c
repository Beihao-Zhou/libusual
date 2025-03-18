/*
 * Primitive slab allocator.
 *
 * Copyright (c) 2007-2009  Marko Kreen, Skype Technologies OÜ
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <usual/slab.h>

#include <usual/slab_internal.h>

#include <string.h>

#include <usual/statlist.h>

#ifndef USUAL_FAKE_SLAB


static void slab_list_append(struct Slab *slab)
{
#ifndef _REENTRANT
	statlist_append(&slab_list, &slab->head);
#endif
}

static void slab_list_remove(struct Slab *slab)
{
#ifndef _REENTRANT
	statlist_remove(&slab_list, &slab->head);
#endif
}

static void init_slab_and_store_in_list(struct Slab *slab, const char *name, unsigned obj_size,
		      unsigned align, slab_init_fn init_func,
		      CxMem *cx)
{
	init_slab(slab, name, obj_size, align, init_func, cx);
	slab_list_append(slab);
}

/* make new slab */
struct Slab *slab_create(const char *name, unsigned obj_size, unsigned align,
			 slab_init_fn init_func,
			 CxMem *cx)
{
	struct Slab *slab;

	/* new slab object */
	slab = cx_alloc0(cx, sizeof(*slab));
	if (slab)
		init_slab_and_store_in_list(slab, name, obj_size, align, init_func, cx);
	return slab;
}

/* free all storage associated by slab */
void slab_destroy(struct Slab *slab)
{
	if (!slab)
		return;

	slab_list_remove(slab);
	slab_destroy_internal(slab);
}

/* add new block of objects to slab */
static void grow(struct Slab *slab)
{
	unsigned count, i, size;
	char *area;
	struct SlabFrag *frag;

	/* calc new slab size */
	count = slab->total_count;
	if (count < 50)
		count = 16 * 1024 / slab->final_size;
	if (count < 50)
		count = 50;
	size = count * slab->final_size;

	/* allocate & init */
	frag = cx_alloc0(slab->cx, size + sizeof(struct SlabFrag));
	if (!frag)
		return;
	list_init(&frag->head);
	area = (char *)frag + sizeof(struct SlabFrag);

	/* init objects */
	for (i = 0; i < count; i++) {
		void *obj = area + i * slab->final_size;
		struct List *head = (struct List *)obj;
		list_init(head);
		statlist_append(&slab->freelist, head);
	}

	/* register to slab */
	slab->total_count += count;
	statlist_append(&slab->fraglist, &frag->head);
}

/* get free object from slab */
void *slab_alloc(struct Slab *slab)
{
	struct List *item = statlist_pop(&slab->freelist);
	if (!item) {
		grow(slab);
		item = statlist_pop(&slab->freelist);
	}
	if (item) {
		if (slab->init_func)
			slab->init_func(item);
		else
			memset(item, 0, slab->final_size);
	}
	return item;
}

/* put object back to slab */
void slab_free(struct Slab *slab, void *obj)
{
	struct List *item = obj;
	list_init(item);
	statlist_prepend(&slab->freelist, item);
}

/* total number of objects allocated from slab */
int slab_total_count(const struct Slab *slab)
{
	return slab->total_count;
}

/* free objects in slab */
int slab_free_count(const struct Slab *slab)
{
	return statlist_count(&slab->freelist);
}

/* number of objects in use */
int slab_active_count(const struct Slab *slab)
{
	return slab_total_count(slab) - slab_free_count(slab);
}

/* call a function for all active slabs */
void slab_stats(slab_stat_fn cb_func, void *cb_arg)
{
	struct Slab *slab;
	struct List *item;

	statlist_for_each(item, &slab_list) {
		slab = container_of(item, struct Slab, head);
		run_slab_stats(slab, cb_func, cb_arg);
	}
}

#else

struct Slab {
	int size;
	struct StatList obj_list;
	slab_init_fn init_func;
	CxMem *cx;
};


struct Slab *slab_create(const char *name, unsigned obj_size, unsigned align,
			     slab_init_fn init_func,
			     CxMem *cx)
{
	struct Slab *s = cx_alloc(cx, sizeof(*s));
	if (s) {
		s->size = obj_size;
		s->init_func = init_func;
		s->cx = cx;
		statlist_init(&s->obj_list, "obj_list");
	}
	return s;
}

void slab_destroy(struct Slab *slab)
{
	struct List *el, *tmp;
	statlist_for_each_safe(el, &slab->obj_list, tmp) {
		statlist_remove(&slab->obj_list, el);
		cx_free(slab->cx, el);
	}
	cx_free(slab->cx, slab);
}

void *slab_alloc(struct Slab *slab)
{
	struct List *o;
	void *res;
	o = cx_alloc(slab->cx, sizeof(struct List) + slab->size);
	if (!o)
		return NULL;
	list_init(o);
	statlist_append(&slab->obj_list, o);
	res = (void *)(o + 1);
	if (slab->init_func)
		slab->init_func(res);
	return res;
}

void slab_free(struct Slab *slab, void *obj)
{
	if (obj) {
		struct List *el = obj;
		statlist_remove(&slab->obj_list, el - 1);
		cx_free(slab->cx, el - 1);
	}
}

int slab_total_count(const struct Slab *slab) { return 0; }
int slab_free_count(const struct Slab *slab) { return 0; }
int slab_active_count(const struct Slab *slab) { return 0; }
void slab_stats(slab_stat_fn cb_func, void *cb_arg) {}


#endif
