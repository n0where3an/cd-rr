#ifndef __CONFIG_H__
#define __CONFIG_H__

// от этих я уже отказался:
#undef RR_PROBE_MODE				/* пробный расчёт небольшого кусочка сектора */

// эти уже работают:
/*#define RR_FULLPATH_ONLY	*/		/* не задавать относительные пути */
/*#define RR_OPEN_ON_DEMAND */		/* открывать файлы не все скопом, */ \
														/* а только по необходимости      */
#define RR_FLIST_ITSELF			/* в rr-filelist'е отмечать его имя явно, */ \
														/* переписывать входной в target-dir      */
#define RR_CHECK						/* приманка - блок, заполненный нулями, */ \
														/* помеченный как битый                 */
#define RR_1PASS_COUNT			/* option: кол-во секторов, к-е можно исправить */ \
														/* при первом проходе, 0 - не пытаться вовсе    */
#define RR_VCD_AS_ISO		/* разрешить обрабатывать файлы со структурой, */ \
												/* похожей на mode2form2 (напр., vcd) как ISO  */
#undef RR_HELP_OPTS			/* явный список опций в help'е */
#define RR_FILL_BAD			/* сбойный сектор: сразу заполнять копию сектора нулями */
#define RR_KEEP_BAD			/* не удалять недовосстановленные файлы */
#define RR_RENAME_BAD		/* недовосстановленные файлы переименовывать в "*.bad" */
#define RR_DMG_ENTRIES	/* задавать битые секторы/байты записями в RR-list'е */
#define RR_UPDATE_DIR	/* каталог с более свежими версиями файлов */
#define RR_COPY_ONLY	/* защищённые файлы только копировать, не восстанавливая */
#define RR_CHECK_ALWAYS	/* всегда выполнять расширенную проверку */
#define RR_ENC_TGTBASE	/* rrenc: куда писать rrlist/rrdata */
#define RR_SHOW_LOST		/* отдельно показывать отсутствующие секторы */
#define RR_FIN_WKARND		/* workaround ошибки в конце последнего файла */
#define RR_ERRORLEVEL	/* подсказка по кодам завершения */
#define RR_EMPTY_DELD	/* трактовать файл нулевой длины в update-dir как удалённый */
#define RR_NO_INIT_READ	/* не читать файл при определении xcd/non-xcd */
#define RR_LEGALZERO	/* allow zero in len and/or time */
#define RR_OEM_API		/* setfileapistooem */
#define RR_BASE_EXT   /* allow m2cdm files with multiple extensions */

// under construction
//define RR_NO_RESTORE /* make m2cdm input list w/o disc restore onto HDD */
#define RR_FLUSH_OUTPUT
//#define RR_OUTPUT_REDIR /* change form of stdout while redirected */
#define RR_CUSTOM_XCD_EXT /* allow changing XCD-files ext from rrt-file */
#define RR_KEEP_OLD_EXT /* keep old extension of media files (as m2cdm's "-x") */
#define RR_ALLOW_ABSPATH /* allow absolute paths in input list */
//#define RR_CHECK_OLDMODE /* allow checking in old compatible mode */
//#define RR_OVERWRITE_OLDMODE /* allow target overwriting in old compatible mode */
//#define RR_LIST_2PASS	/* process filelist in separate section */
#undef RR_STORE_EDC		/* сохранять EDC от массива EDC всех секторов файла */
//#define RR_NEW_ENC		/* encode путём _де_кодирования */
#define RR_DEC_PATH		/* в rrdec брать файл из пути при имени, ложить в путь при -d */
#define RR_IGNORE_DIR /* в rrenc игнорировать записи "-d <dir>" */
#undef RR_BARE_OUTPUT	/* вывод в stdout в простом виде (для front-end'а)*/
#define RR_CHECK_ISO1	/* autocheck for ISO Level 1 compliance */
// todo
#ifndef RR_LIST_2PASS
	#undef RR_TRACK_PATH	/* отслеживать вложенность путей */
#endif /* RR_LIST_2PASS */
#undef RR_PROTECT_RRT	/* защищать .rrt-файл */

#undef RR_VERBOSE_OPTS	/* при "-v" вывести задействованные опции */
#undef RR_1PASS_MATCH	/* выделять под буфер для 1-го прохода сколько нужно */
#undef RR_SUFFICIENT	/* не читать rr-data до конца - брать ровно сколько нужно */
#undef RR_KEEP_EXISTING	/* если в tgt-dir уже есть файл - не бахать */
#undef RR_HARDLINKS		/* несколько имён у одного файла */
#undef RR_GEN_RRLIST	/* rrlist не копировать, а генерировать */
#undef RR_OLD_RRLIST	/* в rrdec: делать rr-filelist в old mode */
#undef RR_INPLACE			/* восстанавливать файлы по месту */
#undef RR_FROMTO_NAME	/* принимать "<from-name> ">" <to_name>" */
#undef RR_ABS_NAME		/* принимать имена в виде ":<abs-name>" */
#undef RR_DMG_LIST		/* метить битые секторы по списку */
/////////////////////////////////////////////////////////////////////////////
#ifdef _MSC_VER
	#define __WIN32__ _WIN32
#endif

#ifndef __WIN32__
	#include <limits.h>

	/* PATH_MAX is not defined in limits.h on some platforms */
	#ifndef PATH_MAX
	#define PATH_MAX 4096
	#endif
    #define _MAX_PATH PATH_MAX
    #define _MAX_EXT 256
    #define _MAX_DRIVE 3
    #define _MAX_DIR _MAX_PATH
    #define _MAX_FNAME _MAX_PATH 

//////////////// file /////////////
    #define _open open
    #define _close close
    #define _read read
    #define _write write
    #define _lseek lseek
	#define _stat stat
	#define _chsize ftruncate

    #define _O_CREAT O_CREAT
    #define _O_RDONLY O_RDONLY
    #define _O_RDWR O_RDWR
    #define _O_TRUNC O_TRUNC
    #define _O_EXCL O_EXCL

    #define _O_BINARY 0
    #define _O_RANDOM 0
    #define _S_IREAD  S_IREAD
    #define _S_IWRITE S_IWRITE
    #define _S_IFMT   S_IFMT
    #define _S_IFREG  S_IFREG
    #define _S_IFDIR  S_IFDIR
#endif

/////////////////////////////////////////////////////////////////////////////
#endif /* __CONFIG_H__ */


#ifndef RR_LIST_2PASS
#else /* RR_LIST_2PASS */
#endif /* RR_LIST_2PASS */
