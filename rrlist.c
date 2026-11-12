//rrlist_root_t *root;
#include "rrfile.h"
#include "rrl_int.h"

#ifdef RR_LIST_2PASS

void rr_nomem_msg( char *prefix, size_t size ) {
	rr_printmsg( VERBOSE_QUIET,
		"%s%snot enough memory to get %d bytes\n",
		( prefix == NULL ) ? "" : prefix,
		( prefix == NULL ) ? "" : ": ",
		size );
}
rrlist_tree_t *rrlist_make_tree( const char *name, rrlist_mode_t mode ) {
	// возвращает указатель на root-dir
	rrlist_tree_t *tree;
	int size = sizeof( rrlist_tree_t ) + 1;
	if ( ( tree = malloc( size ) ) == NULL ) {
		rr_nomem_msg( "RR-list", size );
		return ( NULL );
	}
	strcpy( tree->tree_name, name );
	INIT_LIST_HEAD( &tree->all_dirs );
	INIT_LIST_HEAD( &tree->all_files );
	INIT_LIST_HEAD( &tree->all_inodes );

	INIT_LIST_HEAD( &tree->root.subdirs );
	INIT_LIST_HEAD( &tree->root.files );
#ifndef RRLIST_MAX_ALIASES
	tree->max_aliases =
#endif /* RRLIST_MAX_ALIASES */
#ifdef RRLIST_COUNTS
		tree->root.file_count = tree->root.subdir_count =
#endif /* RRLIST_COUNTS */
		tree->root.depth = 0;
	INIT_LIST_HEAD( &tree->root.all_dirs );
	INIT_LIST_HEAD( &tree->root.subdir_of_parent );
	tree->root.tree = tree;
	tree->root.parent = tree->last_dir = NULL;
	tree->last_dirname[ 0 ] = tree->last_comment[ 0 ] = '\0';
	tree->mode = mode;
	tree->root.shortname = tree->root.fullname = tree->root.strings;
	tree->root.strings[ 0 ] = 0;
	tree->root.comments = NULL;
	tree->comment_len = 0;
	return ( tree );
}
int rrlist_add_comment( rrlist_tree_t *tree, const char *comment ) {
	int len;
	if ( comment == NULL )	// просто вставляем пустую строку
		comment = "";
	len = min( strlen( comment ),
		( RRLIST_MAX_COMMENT_LENGTH - 1 ) - tree->comment_len );
	if ( len > 0 ) {
		char *newpos = tree->last_comment + tree->comment_len;
		memcpy( newpos, comment, len ); newpos += len;
		strcpy( newpos, "\n" );
		tree->comment_len += len + 1;
	} else if ( len < 0 )
		len = 0;
	return ( len );
}
static rrlist_dir_t *create_subdir( rrlist_dir_t *parent, char *shortname ) {
	rrlist_dir_t *dir;
	char fullname[ _MAX_PATH ];
	int fullnamelen, shortnamelen, diritemsize;
	_makepath( fullname, NULL, parent->fullname, shortname, NULL );
	fullnamelen = strlen( fullname );
	shortnamelen = strlen( shortname );
	diritemsize = sizeof( rrlist_dir_t ) + fullnamelen + shortnamelen +
		parent->tree->comment_len + 3;
	if ( ( dir = malloc( diritemsize ) ) == NULL ) {
		rr_nomem_msg( "RR-list", diritemsize );
		return ( NULL );
	}
// инициализируем:
	INIT_LIST_HEAD( &dir->files );
	INIT_LIST_HEAD( &dir->subdirs );
	dir->processed = 0;
#ifdef RRLIST_COUNTS
	dir->file_count = dir->subdir_count = 0;
#endif /* RRLIST_COUNTS */
	dir->parent = parent; dir->tree = parent->tree;
	dir->shortname = dir->strings; strcpy( dir->shortname, shortname );
	dir->fullname = dir->shortname + shortnamelen + 1;
	strcpy( dir->fullname, fullname );
	if ( dir->tree->comment_len > 0 ) {
		dir->comments = dir->fullname + fullnamelen + 1;
		strcpy( dir->comments, dir->tree->last_comment );
		dir->tree->comment_len = 0;
		dir->tree->last_comment[ 0 ] = '\0';
	} else
		dir->comments = NULL;
	dir->depth = parent->depth + 1;
// включаем в списки:
	list_add_tail( &dir->all_dirs, &dir->tree->all_dirs );
	list_add_tail( &dir->subdir_of_parent, &parent->subdirs );
#ifdef RRLIST_COUNTS
	dir->parent->subdir_count++;
#endif /* RRLIST_COUNTS */
	return ( dir );
}

rrlist_dir_t *rrlist_add_dir( rrlist_tree_t *tree, const char *dirname ) {
// возвращает в конце концов каталог, maybe свежесозданный
	char
		realdir[ _MAX_DIR ] /*, name[ _MAX_FNAME ], ext[ _MAX_EXT ],
		drive[ _MAX_DRIVE ], tmppath[ _MAX_PATH ]*/;
	rrlist_dir_t *dir; char *level;
	int newly_created = 0;
	struct list_head *jp;
	if ( ( tree->last_dir != NULL ) &&	// кэш не пуст
		!strcmp( dirname, tree->last_dirname ) ) {	// есть в кэше
		return ( tree->last_dir );
	}
// анализируем:
	strcpy( realdir, dirname ); dir = &tree->root;	// есть всегда
	level = strtok( realdir, "/\\" );
	while ( level != NULL ) {	// ищем подходящий
		rrlist_dir_t *subdir = NULL;
		list_for_each( jp, &dir->subdirs ) {
			rrlist_dir_t *jd = list_entry( jp, rrlist_dir_t, subdir_of_parent );
			if ( !strcmp( level, jd->shortname ) ) {	// нашли
				subdir = jd; break;
			}
		}
		if ( subdir == NULL ) {	// не нашли - делаем
			subdir = create_subdir( dir, level );
			if ( newly_created ) {	// его родитель - тоже свежесозданный
				subdir->comments = dir->comments;
				dir->comments = NULL;
			} else
				newly_created = 1;
		}
		dir = subdir;
		level = strtok( NULL, "/\\" );
	}
// готово:
	tree->last_dir = dir;
	strcpy( tree->last_dirname, dirname );
	if ( newly_created ) {
	}
	return ( dir );
}
rrlist_file_t *rrlist_add_alias(
		rrlist_tree_t *tree, char *fullname, char type ) {
	return ( NULL );
}
static void del_subtree( rrlist_dir_t *dir ) {
	struct list_head *jp;
	rrlist_dir_t *jd = NULL;
// сносим подкаталоги:
	list_for_each( jp, &dir->subdirs ) {
		rrlist_dir_t *td = list_entry( jp, rrlist_dir_t, all_dirs );
		if ( jd != NULL ) free( jd );	// бахнем предыдущий (если есть)
		jd = td;
	// снесём под-подкаталоги:
		del_subtree( jd );
	// удалим подкаталог из списка "все каталоги"
		list_del( /*&jd->all_dirs*/ jp );
		list_del( &jd->subdir_of_parent );	// и из родительского (dir)
	}
	if ( jd != NULL ) free( jd );	// бахнем последний
#ifdef RRLIST_COUNTS
	dir->subdir_count = 0;
#endif /* RRLIST_COUNTS */
}

void rrlist_erase_tree( rrlist_tree_t *tree ) {
	struct list_head *jp;
	rrlist_file_t *jf = NULL; rrlist_inode_t *ji = NULL;
// сначала перебахаем файлы:
	list_for_each( jp, &tree->all_files ) {
		rrlist_file_t *tf = list_entry( jp, rrlist_file_t, all_files );
		if ( jf != NULL ) free( jf );	// бахнем предыдущий (если есть)
		jf = tf;
	// удалим файл из всех списков:
		list_del( /*&jf->all_files*/ jp );	// из списка "все файлы"
		list_del( &jf->file_of_dir );				// из подкаталога
		list_del( &jf->file_of_inode );			// из линков к иноду
#ifdef RRLIST_COUNTS
		jf->dir->file_count--;
		jf->inode->link_count--;
#endif /* RRLIST_COUNTS */
	}
	if ( jf != NULL ) free( jf );	// бахнем последний
// сносим иноды:
	list_for_each( jp, &tree->all_inodes ) {
		rrlist_inode_t *ti = list_entry( jp, rrlist_inode_t, all_inodes );
		if ( ji != NULL ) free( ji );	// бахнем предыдущий (если есть)
		ji = ti;
	// удалим иноду из списка "все иноды"
		list_del( /*&jf->all_files*/ jp );
	}
	if ( ji != NULL ) free( ji );	// бахнем последний
// самое сложное - зачистка каталогов (дерево)
	del_subtree( &tree->root );
	free( tree );
}
rrlist_file_t *rrlist_add_file(	// add file into tree - into last created dir
		rrlist_tree_t *tree, const char *fullname,
		const char *realname,
		time_t time, size_t size, char type ) {
	char
		realdir[ _MAX_DIR ], name[ _MAX_FNAME ], ext[ _MAX_EXT ],
		drive[ _MAX_DRIVE ], tmppath[ _MAX_PATH ];
	char file_fullname[ _MAX_PATH ], file_shortname[ _MAX_PATH ],
		file_realname[ _MAX_PATH ];
	int fullnamelen, shortnamelen, realnamelen;
	unsigned long realname_edc;
	int fileitemsize, inodeitemsize = 0;
	rrlist_dir_t *parent;
	rrlist_file_t *file = NULL;
	rrlist_inode_t *inode = NULL;
	struct list_head *jp;

	_splitpath( fullname, drive, tmppath, name, ext );
	_makepath( realdir, drive, tmppath, NULL, NULL );
	switch ( tree->mode ) {
		default:
		case DIR_EXPLICIT: {
			if ( ( parent = tree->last_dir ) == NULL )
				parent = &tree->root;
		} break;
		case DIR_EXTRACT: {
			parent = rrlist_add_dir( tree, tmppath );
		} break;
	}
	_makepath( file_fullname, NULL, parent->fullname, name, ext );
	_makepath( file_shortname, NULL, NULL, name, ext );
// делаем realname
	_fullpath( file_realname, ( realname == NULL ) ? file_fullname : realname,
		_MAX_PATH );

	fullnamelen = strlen( file_fullname );
	shortnamelen = strlen( file_shortname );
	realnamelen = strlen( file_realname );
	realname_edc = build_edc( file_realname, 0, realnamelen );
// поиск подходящего inode'а:
	if ( realname != NULL ) list_for_each( jp, &tree->all_inodes ) {
		rrlist_inode_t *ji = list_entry( jp, rrlist_inode_t, all_inodes );
		if ( ( ji->name_edc == realname_edc ) &&
				( strcmp( ji->name_edc, file_realname ) == 0 ) ) {	// ура, идентичные
			inode = ji;
			break;
		}
	}

	fileitemsize = sizeof( rrlist_file_t ) + fullnamelen + shortnamelen +
		tree->comment_len + 3;
	if ( inode == NULL )
		inodeitemsize = sizeof( rrlist_inode_t ) + realnamelen + 1;
	if ( ( ( file = malloc( fileitemsize ) ) == NULL )
			|| ( ( inodeitemsize != NULL ) &&
			( ( inode = malloc( inodeitemsize ) ) == NULL ) ) ) {
		rr_nomem_msg( "RR-list", fileitemsize + inodeitemsize );
		goto fatal;
	}
	if ( inodeitemsize != 0 ) {	// свежестянутый
		memset( inode, 0, inodeitemsize );
		inode->type = type; inode->size = size; inode->time = time;
		INIT_LIST_HEAD( &inode->links );
#ifdef RRLIST_COUNTS
		inode->link_count = 0;
#endif /* RRLIST_COUNTS */
		inode->realname = inode->strings;
		strcpy( inode->realname, file_realname );
		inode->name_edc = realname_edc;
		list_add_tail( &inode->all_inodes, &parent->tree->all_inodes );
//	} else {
//#error "Включить в кольцо, выставить type"
	}

	file->fullname = file->strings; strcpy( file->fullname, file_fullname );
	file->shortname = file->fullname + fullnamelen + 1;
	strcpy( file->shortname, file_shortname );
	if ( tree->comment_len > 0 ) {
		file->comments = file->shortname + shortnamelen + 1;
		strcpy( file->comments, tree->last_comment );
		tree->comment_len = 0; tree->last_comment[ 0 ] = '\0';
	} else
		file->comments = NULL;
	file->dir = parent; file->inode = inode;
	list_add_tail( &file->file_of_dir, &parent->files );
	list_add_tail( &file->file_of_inode, &inode->links );
#ifdef RRLIST_COUNTS
	parent->file_count++;
#endif /* RRLIST_COUNTS */
#ifndef RRLIST_MAX_ALIASES
	#ifdef RRLIST_COUNTS
	if ( ++inode->link_count > tree->max_aliases )
		tree->max_aliases = inode->link_count;
	#else /* RRLIST_COUNTS */
		#error "neither RRLIST_COUNTS nor RRLIST_MAX_ALIASES defined"
	#endif /* RRLIST_COUNTS */
#endif /* RRLIST_MAX_ALIASES */
	list_add_tail( &file->all_files, &parent->tree->all_files );
	return ( file );
fatal:
	if ( file != NULL ) free( file );
	if ( ( inodeitemsize != 0 ) && ( inode != NULL ) ) free( inode );
	return ( NULL );
}
rrlist_file_t *rrlist_next_file( rrlist_file_t *file ) {
	rrlist_file_t *nextfile = NULL;
	rrlist_dir_t *dir = file->dir;
	struct list_head *jp = file->file_of_dir.next;
	do {
		if ( jp != &dir->files ) {	// есть ещё файлы
			nextfile = list_entry( jp, rrlist_file_t, file_of_dir );
			break;	// готово
		}
// каталог весь
		if ( !list_empty( &dir->subdirs ) ) {	// уходим глубже
			dir = list_entry( &dir->subdirs.next, rrlist_dir_t, subdir_of_parent );
			jp = dir->files.next;
			continue;
		}
next_subdir:
// глубже некуда - уходим в соседний каталог
//#error "продолжить здесь"
		if ( dir->parent == NULL )	// это был корневой, искать больше неча
			break;
// уходим в следующий подкаталог
		if ( dir->subdir_of_parent.next != &dir->parent->subdirs ) {	// соседний есть
			dir = list_entry( &dir->subdir_of_parent.next, rrlist_dir_t, subdir_of_parent );
			jp = dir->files.next;
			continue;
		}
// уходим в каталог, следующий после родительского
		dir = dir->parent; goto next_subdir;	// каталог уже обрабатывался
	} while ( file == NULL );
	return ( file );
}

static int process_file( rrlist_file_t *file,
		process_file_func_t *file_func, void *opaque ) {
	char *aliases[ RRLIST_MAX_ALIASES + 1 ];
	rrlist_inode_t *inode = file->inode;
	int jx = 0;
	struct list_head *jp;
	if ( RRL_HAS_ALIASES( inode ) ) list_for_each( jp, &inode->links ) {
		rrlist_file_t *tfile = list_entry( jp, rrlist_file_t, file_of_inode );
		aliases[ jx++ ] = tfile->fullname;
		if ( jx > RRLIST_MAX_ALIASES )
			break;
	}
	aliases[ jx ] = NULL;
	file_func( opaque, file, file->fullname, file->shortname,
			inode->realname, inode->size, inode->time, inode->type,
			RRL_HAS_ALIASES( inode ) ? aliases : NULL, file->comments );
	return ( 0 );
}

static int process_dir( rrlist_dir_t *dir, process_dir_func_t *dir_func,
		process_file_func_t *file_func, void *opaque ) {
	struct list_head *jp;
	if ( file_func != NULL ) list_for_each( jp, &dir->files ) {
		process_file( list_entry( jp, rrlist_file_t, file_of_dir ),
				file_func, opaque );
	}
	list_for_each( jp, &dir->subdirs ) {
		rrlist_dir_t *jd = list_entry( jp, rrlist_dir_t, subdir_of_parent );
		if ( dir_func != NULL )
			dir_func( opaque, jd, jd->fullname, jd->shortname, jd->depth,
					jd->comments );
		process_dir( jd, dir_func, file_func, opaque );
	}
	return ( 0 );
}
int rrlist_process_tree( rrlist_tree_t *tree, process_dir_func_t *dir_func,
		process_file_func_t *file_func, void *opaque ) {
		if ( dir_func != NULL )
			dir_func( opaque, &tree->root, tree->root.fullname, tree->root.fullname,
					tree->root.depth, tree->root.comments );
	return ( process_dir( &tree->root, dir_func, file_func, opaque ) );
}
int rrlist_process_files( rrlist_tree_t *tree, process_dir_func_t *dir_func,
		process_file_func_t *file_func, void *opaque ) {
	struct list_head *jp;
	rrlist_dir_t *jd = &tree->root;
	jd->processed = 1;
	list_for_each( jp, &tree->all_files ) {
		rrlist_file_t *jf = list_entry( jp, rrlist_file_t, all_files );
		if ( ( dir_func != NULL ) && ( jf->dir != jd ) ) {	// смена каталога
			jd = jf->dir;
			dir_func( opaque, jd, jd->fullname, jd->shortname, jd->depth,
					jd->processed ? NULL : jd->comments );
			jd->processed = 1;
		}
		process_file( jf, file_func, opaque );
	}
	if ( tree->comment_len > 0 ) {	// остались бесхозные комментарии - выводим
		file_func( opaque, NULL, NULL, NULL, NULL, -1, -1,
			'c', NULL, tree->last_comment );
	}
	list_for_each( jp, &tree->all_dirs ) {
		list_entry( jp, rrlist_dir_t, all_dirs )->processed = 0;
	}
	return ( NO_ERROR );
}

#endif /* RR_LIST_2PASS */
