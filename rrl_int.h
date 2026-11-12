#ifndef _RRL_INT_H_
#define _RRL_INT_H_

#include "config.h"

#define RRLIST_MAX_ALIASES 64
#define RRLIST_MAX_COMMENT_LENGTH 4095 /* нетто, т.е. без финального '\0' */
//#undef RRLIST_COUNTS

#ifdef RR_LIST_2PASS

#define __KERNEL__
#define __inline__ __inline
#include "list.h"
#include <stdlib.h>
#include <string.h>
//#include "rrfile.h"
#include "rrlist.h"
#include "rr.h"
#include <sys/types.h>

struct rrlist_dir_s {
	rrlist_tree_t *tree;	// дерево, которому всё это принадлежит
	rrlist_dir_t *parent;	// NULL if root
	struct list_head
		subdirs,	// list of
		files;		// list of
	int
#ifdef RRLIST_COUNTS
		subdir_count,
		file_count,
#endif /* RRLIST_COUNTS */
		processed,
		depth;
	char
		*comments,	// в т.ч. и несколько строк - до RRLIST_MAX_COMMENT_LENGTH
		*shortname,	// last name
		*fullname;	// whole path -- redundant
	struct list_head
		all_dirs,						// ring of all dirs/subdirs
		subdir_of_parent;	// link to parent->subdirs;
	char
		strings[ 0 ];	// tail
};
struct rrlist_tree_s {
	rrlist_mode_t mode;
	struct list_head
		all_dirs,
		all_files,
		all_inodes;
//#define LAST_FILE( tree ) \
//	list_entry( (tree)->all_files.prev, rrlist_file_t, all_files );
	rrlist_dir_t *last_dir;
	int comment_len;	// нетто (без '\0')
#ifndef RRLIST_MAX_ALIASES
	int max_aliases;
#endif /* RRLIST_MAX_ALIASES */
	char tree_name[ _MAX_PATH ];
	char last_dirname[ _MAX_PATH ];
	char last_comment[ RRLIST_MAX_COMMENT_LENGTH + 1 ];
	rrlist_dir_t root;
};

struct rrlist_file_s {
	rrlist_dir_t *dir;
	rrlist_inode_t *inode;
	char
		*comments,	// в т.ч. и несколько строк - до RRLIST_MAX_COMMENT_LENGTH
		*shortname,	// last name only
		*fullname;	// whole path -- redundant
	struct list_head
		all_files,			// whole ring
		file_of_dir,		// link to rrlist_dir_t.files
		file_of_inode;	// link to rrlist_inode_t.files
	char
		strings[ 0 ];	// tail
};
struct rrlist_inode_s {
	struct list_head
		links;
	char *realname;
	unsigned long name_edc;
#ifdef RRLIST_COUNTS
	int link_count;
	#define RRL_HAS_ALIASES( inode ) \
		( (inode)->link_count > 1 )
#else /* RRLIST_COUNTS */
	#define RRL_HAS_ALIASES( inode ) \
		( (inode)->links.next != (inode)->links.prev )
#endif /* RRLIST_COUNTS */
	time_t time;
	size_t size;
	char type;	// 'f' / 'm' / 'i' / 'x'
	struct list_head
		all_inodes;		// whole ring
	char
		strings[ 0 ];	// tail
};
struct rrlist_item_s {
	int is_dir;
	union {
		rrlist_dir_t dir;
		rrlist_file_t file;
	};
};

#endif /* RR_LIST_2PASS */

#endif /* _RRL_INT_H_ */
