#ifndef __RRLIST_H__
#define __RRLIST_H__

#include "config.h"
#include <time.h>

typedef enum {
	DIR_EXTRACT,		// extract dir from name
	DIR_EXPLICIT	// with "-d dir"
} rrlist_mode_t;

#ifdef RR_LIST_2PASS

struct rrlist_comment_s;
struct rrlist_tree_s;
struct rrlist_dir_s;
struct rrlist_file_s;
struct rrlist_inode_s;
typedef struct rrlist_comment_s rrlist_comment_t;
typedef struct rrlist_tree_s rrlist_tree_t;
typedef struct rrlist_dir_s rrlist_dir_t;
typedef struct rrlist_file_s rrlist_file_t;
typedef struct rrlist_inode_s rrlist_inode_t;

rrlist_tree_t *rrlist_make_tree( const char *name, rrlist_mode_t mode );
int rrlist_add_comment( rrlist_tree_t *tree, const char *comment );
rrlist_dir_t *rrlist_add_dir( rrlist_tree_t *tree, const char *dirname );
rrlist_file_t *rrlist_add_file(	// add file into tree - into last selected dir
		rrlist_tree_t *tree, const char *fullname,
		const char *realname,
		time_t time, size_t size, char type );
void rrlist_erase_tree( rrlist_tree_t *tree );
rrlist_file_t *rrlist_first_file( rrlist_tree_t *tree );
rrlist_file_t *rrlist_next_file( rrlist_file_t *file );
// hardlink:
rrlist_file_t *rrlist_add_link( rrlist_tree_t *tree, const char *fullname,
		rrlist_inode_t *inode );	// хм, легко сказать...
// ну, эти тривиальны:
char *rrlist_file_name( rrlist_file_t *file );
rrlist_inode_t *rrlist_file_inode( rrlist_file_t *file );


// callback-функции:
#define RRLIST_DIRFUNC_PARM \
	void *opaque, rrlist_dir_t *dir, const char *fullname, \
	const char *shortname, int depth, const char *comment
#define RRLIST_FILEFUNC_PARM \
		void *opaque, rrlist_file_t *file, \
		const char *fullname, const char *shortname, const char *realname, \
		size_t size, time_t time, char type, const char **aliases, \
		const char *comment
typedef void (process_dir_func_t)( RRLIST_DIRFUNC_PARM );
typedef void (process_file_func_t)( RRLIST_FILEFUNC_PARM ); 


int rrlist_process_tree( rrlist_tree_t *tree, process_dir_func_t *dir_func,
		process_file_func_t *file_func, void *opaque );	// обход дерева
int rrlist_process_files( rrlist_tree_t *tree, process_dir_func_t *dir_func,
		process_file_func_t *file_func, void *opaque );	// тупо вдоль списка файлов

#endif /* RR_LIST_2PASS */

#endif /* __RRLIST_H__ */
