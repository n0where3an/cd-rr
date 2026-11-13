/* SPDX-License-Identifier: AGPL-3.0-or-later */
/*
 * list.h - intrusive circular doubly linked list.
 *
 * A list is a sentinel `struct list_head`; an empty list points to itself.
 * Items embed a `struct list_head` member and are recovered from it with
 * list_entry().
 *
 * Contract relied upon by the callers: list_del() unlinks an entry from its
 * neighbours but leaves the entry's own next/prev untouched, so the
 * list_for_each() loop may delete the current entry and continue.
 */
#ifndef RR_LIST_H
#define RR_LIST_H

#include <stddef.h>

#if defined( _MSC_VER ) && !defined( __cplusplus )
	#define RR_LIST_INLINE static __inline
#else
	#define RR_LIST_INLINE static inline
#endif

struct list_head {
	struct list_head *next, *prev;
};

/* static initializer for an empty list */
#define LIST_HEAD_INIT( name ) { &(name), &(name) }

/* defines and initializes an empty list */
#define LIST_HEAD( name ) \
	struct list_head name = LIST_HEAD_INIT( name )

/* makes an empty list at run time */
#define INIT_LIST_HEAD( head ) \
	do { \
		(head)->next = (head); \
		(head)->prev = (head); \
	} while (0)

/* links `item` between two adjacent entries */
RR_LIST_INLINE void rr_list_link( struct list_head *item,
		struct list_head *before, struct list_head *after ) {
	item->prev = before;
	item->next = after;
	before->next = item;
	after->prev = item;
}

/* inserts `item` right after `head` (at the front of the list) */
RR_LIST_INLINE void list_add( struct list_head *item, struct list_head *head ) {
	rr_list_link( item, head, head->next );
}

/* inserts `item` right before `head` (at the back of the list) */
RR_LIST_INLINE void list_add_tail( struct list_head *item, struct list_head *head ) {
	rr_list_link( item, head->prev, head );
}

/* unlinks `item`; item->next/prev keep their values (see the contract above) */
RR_LIST_INLINE void list_del( struct list_head *item ) {
	item->prev->next = item->next;
	item->next->prev = item->prev;
}

RR_LIST_INLINE int list_empty( const struct list_head *head ) {
	return ( head->next == head );
}

/* moves all entries of `list` to the front of `head`; `list` is left stale */
RR_LIST_INLINE void list_splice( struct list_head *list, struct list_head *head ) {
	if ( !list_empty( list ) ) {
		struct list_head *first = list->next, *last = list->prev;
		struct list_head *after = head->next;
		first->prev = head;
		head->next = first;
		last->next = after;
		after->prev = last;
	}
}

/* pointer to the structure of `type` whose `member` is `ptr` */
#define list_entry( ptr, type, member ) \
	( (type *) ( (char *) (ptr) - offsetof( type, member ) ) )

/* iterates `pos` over all entries of `head` */
#define list_for_each( pos, head ) \
	for ( (pos) = (head)->next; (pos) != (head); (pos) = (pos)->next )

#endif /* RR_LIST_H */
