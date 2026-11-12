/*

    2012/11/02 - 1.5.1e - Elicit Gingers

    mkvcdfs.c: some function prototypes included to avoid stupid warning

    2007/08/04 - 1.5.1d - Elicit Gingers

    vcdisofs.c: preserve original timestamps of files placed on disc

    2005/05/18 - 1.5.1b - Elicit Gingers

    vcdisofs.c: fixed OEM filenames processing

    2003/02/01 - 1.5.1 - DeXT
    
    vcdisofs.c: fixed file names when using relative paths & ':' character only

    2002/10/13 - 1.5 - DeXT
    
    vcdisofs.c: each directory can now contain an unlimited number of files
                (it was previously limited to about 32 per directory)
    vcdisofs.c: added nested subdirectories support
    vcdisofs.c: better use of ISO track space (files are being recorded right
                after the ISO FS)
    vcdisofs.c: directory record buffers are now allocated at run-time
    vcdisofs.c: fixed international character set support for subdirs, too

    2002/08/26 - 1.4.2 - ML & DeXT
    
    vcdisofs.c: fixed character set for international file names

    2002/06/20 - 1.4 - DeXT
    
    vcdisofs.c: subdirs now work for form2 files, too
    vcdisofs.c: form2 original extension can now be kept
    vcdisofs.c: fixed max filename length calculation (was greater than allowed)
    vcdisofs.c: ISO building is now faster for form1 files (no need to reserve space)
    
    2002/05/23 - 1.3 - DeXT
    
    vcdisofs.c: added subdirectories support (for Form1 files only)
    vcdisofs.c: added ASCII character set support

    2002/05/17 - 1.2 - DeXT
    
    vcdisofs.c: added Form1 files support

    2002/05/06 - 1.2pre2 - DeXT
    
    vcdisofs.c: improved XA compatibility (added XA sig for every file/dir)
    vcdisofs.c: added long file names support (ISO-9660 Level 2)
    vcdisofs.c: fixed emtpy block in ISO track
    
    2002/05/04 - 1.2pre - Nic
    
    vcdisofs.c: M2F2 file extension is no longer fixed

    2002/04/18 - beta 1.1 - DeXT
    
    vcdisofs.c: lowered ISO track size to 302 sectors (~700 Kb)

    2002/04/17 - beta 1.0 - DeXT

    vcdisofs.c: fixed make_path_tables() to not add root files as dirs
    vcdisofs.c: DAT files are added to the root directory instead of MPEGAV
    vcdisofs.c: removed unneeded VCD stuff

*/

/*
 * Make a (simple) ISO 9660 filesystem for a Video CD
 *
 * Many things in this program are just taken from mkisofs
 *
 * Structure of the filesystem
 *
 *     Block      Content
 *
 *     0-15       empty
 *     16         Primary Volume descriptor
 *     17         End Volume descriptor
 *     18         Path table (Intel Byte order) (must fit in 1 Sector)
 *     19         Path table (Motorola Byte order) (must fit in 1 Sector)
 *     20         Root directory
 *     21  ...    Other directories
 *     150 ...    Files in directory VCD
 *     210 ...    Other files
 */

#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <time.h>
#include <assert.h>
#include "defaults.h"
#include "funcs.h"

/* Path table stuff. Note that this is simplified and only valid
   if there are no subdirectories below the directories in the
   root directory and if the path tables fit into 1 sector */

#define PVD_EXTENT 16
#define EVD_EXTENT 17
#define PATH_TABLE_L_EXTENT 18
#define PATH_TABLE_M_EXTENT 19
#define ROOT_DIR_EXTENT   20

#define ISO_DIR_SIZE 2048

static char path_table_l[2048];
static char path_table_m[2048];
static int  path_table_size;

static char zero2048[2048] = { 0, };
static char buff2048[2048];

/* Various files on the VCD */

static char entries_file[2048];
static char info_file[2048] = {
  'V', 'I', 'D', 'E', 'O', '_', 'C', 'D',
    1,   1, '1', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  ' ', ' ',   0,   1,   0,   1,   0,   0,
};

static struct tm dt;	// disc creation time -- by EG
int gmt_off;	// offset from GMT in 15-min units -- by EG

#define BCD(x) ( ((x)/10)*16 + (x)%10 )

static void set_721( char *pnt, unsigned int i)
{
     pnt[0] = i & 0xff;
     pnt[1] = (i >> 8) &  0xff;
}

static void set_722( char *pnt, unsigned int i)
{
     pnt[0] = (i >> 8) &  0xff;
     pnt[1] = i & 0xff;
}

static void set_723( char *pnt, unsigned int i)
{
     pnt[3] = pnt[0] = i & 0xff;
     pnt[2] = pnt[1] = (i >> 8) &  0xff;
}

static void set_731( char *pnt, unsigned int i)
{
     pnt[0] = i & 0xff;
     pnt[1] = (i >> 8) &  0xff;
     pnt[2] = (i >> 16) &  0xff;
     pnt[3] = (i >> 24) &  0xff;
}

static void set_732( char *pnt, unsigned int i)
{
     pnt[3] = i & 0xff;
     pnt[2] = (i >> 8) &  0xff;
     pnt[1] = (i >> 16) &  0xff;
     pnt[0] = (i >> 24) &  0xff;
}

static void set_733( char *pnt, unsigned int i)
{
     pnt[7] = pnt[0] = i & 0xff;
     pnt[6] = pnt[1] = (i >> 8) &  0xff;
     pnt[5] = pnt[2] = (i >> 16) &  0xff;
     pnt[4] = pnt[3] = (i >> 24) &  0xff;
}

static void set_str( char *pnt, char *src, unsigned int len)
{
     int i, l;
     l = strlen(src);

     for(i=0;i<len;i++) pnt[i] = (i<l) ? src[i] : ' ';
}

void make_path_tables(dir_s **dir_entry, int num_dirs)
{
   int i, j, len;

   memset(path_table_l, 0, sizeof(path_table_l));
   memset(path_table_m, 0, sizeof(path_table_m));

   path_table_size = 0;
   
   for(i = 0; i < num_dirs; i++)
   {
      len = strlen(dir_entry[i]->name);
      if (len == 0) len = 1; // root dir

      if (path_table_size + 8 + (len & 1 ? len + 1 : len) > sizeof(path_table_l)) // avoid crossing path table boundary
         { fprintf(stderr,"Fatal Error: too many dirs\n"); exit(1); }

      path_table_l[path_table_size] = len;
      path_table_m[path_table_size] = len;
      path_table_size += 2;

      set_731(path_table_l + path_table_size, dir_entry[i]->extent);
      set_732(path_table_m + path_table_size, dir_entry[i]->extent);
      path_table_size += 4;

      set_721(path_table_l + path_table_size, dir_entry[i]->parent_num);
      set_722(path_table_m + path_table_size, dir_entry[i]->parent_num);
      path_table_size += 2;

      for(j=0; j<len; j++)
      {
         path_table_l[path_table_size] = dir_entry[i]->name[j];
         path_table_m[path_table_size] = dir_entry[i]->name[j];
         path_table_size++;
      }

      if(path_table_size & 1) path_table_size++;
   }
}

/*
void make_path_tables(dir_s **dir_entry)
{
   int i, j, len, extent, next_entry;
   unsigned char *root_dir_entry;

   memset(path_table_l, 0, sizeof(path_table_l));
   memset(path_table_m, 0, sizeof(path_table_m));

   path_table_size = 0;

   root_dir_entry = dir_entry[0]->record;

   for(i = 0; i < dir_entry[0]->len; i += next_entry)
   {
      next_entry = root_dir_entry[i]; // offset for next entry
      if (next_entry == 0) {
         next_entry = ISO_DIR_SIZE-(i%ISO_DIR_SIZE); // skip to next block boundary
         continue; }

      if (root_dir_entry[i+33]==1) continue; // skip .. in root
      if (root_dir_entry[i+25]==0) continue; // skip files

      len = root_dir_entry[i+32];

      if (path_table_size + 8 + (len & 1 ? len + 1 : len) > sizeof(path_table_l)) // avoid crossing path table boundary
         { fprintf(stderr,"Fatal Error: too many dirs\n"); exit(1); }

      path_table_l[path_table_size] = len;
      path_table_m[path_table_size] = len;
      path_table_size += 2;

      extent =  root_dir_entry[i+2]      +
               (root_dir_entry[i+3]<< 8) +
               (root_dir_entry[i+4]<<16) +
               (root_dir_entry[i+5]<<24);
      set_731(path_table_l + path_table_size, extent);
      set_732(path_table_m + path_table_size, extent);
      path_table_size += 4;

      set_721(path_table_l + path_table_size, 1);
      set_722(path_table_m + path_table_size, 1);
      path_table_size += 2;

      for(j=0; j<len; j++)
      {
         path_table_l[path_table_size] = root_dir_entry[i+33+j];
         path_table_m[path_table_size] = root_dir_entry[i+33+j];
         path_table_size++;
      }

      if(path_table_size & 1) path_table_size++;
   }
}
*/

/* From mkisofs: */

#define ISODCL(from, to) (to - from + 1)

static struct iso_primary_descriptor {
        char type                       [ISODCL (  1,   1)]; /* 711 */
        char id                         [ISODCL (  2,   6)];
        char version                    [ISODCL (  7,   7)]; /* 711 */
        char unused1                    [ISODCL (  8,   8)];
        char system_id                  [ISODCL (  9,  40)]; /* achars */
        char volume_id                  [ISODCL ( 41,  72)]; /* dchars */
        char unused2                    [ISODCL ( 73,  80)];
        char volume_space_size          [ISODCL ( 81,  88)]; /* 733 */
        char escape_sequences           [ISODCL ( 89, 120)];
        char volume_set_size            [ISODCL (121, 124)]; /* 723 */
        char volume_sequence_number     [ISODCL (125, 128)]; /* 723 */
        char logical_block_size         [ISODCL (129, 132)]; /* 723 */
        char path_table_size            [ISODCL (133, 140)]; /* 733 */
        char type_l_path_table          [ISODCL (141, 144)]; /* 731 */
        char opt_type_l_path_table      [ISODCL (145, 148)]; /* 731 */
        char type_m_path_table          [ISODCL (149, 152)]; /* 732 */
        char opt_type_m_path_table      [ISODCL (153, 156)]; /* 732 */
        char root_directory_record      [ISODCL (157, 190)]; /* 9.1 */
        char volume_set_id              [ISODCL (191, 318)]; /* dchars */
        char publisher_id               [ISODCL (319, 446)]; /* achars */
        char preparer_id                [ISODCL (447, 574)]; /* achars */
        char application_id             [ISODCL (575, 702)]; /* achars */
        char copyright_file_id          [ISODCL (703, 739)]; /* 7.5 dchars */
        char abstract_file_id           [ISODCL (740, 776)]; /* 7.5 dchars */
        char bibliographic_file_id      [ISODCL (777, 813)]; /* 7.5 dchars */
        char creation_date              [ISODCL (814, 830)]; /* 8.4.26.1 */
        char modification_date          [ISODCL (831, 847)]; /* 8.4.26.1 */
        char expiration_date            [ISODCL (848, 864)]; /* 8.4.26.1 */
        char effective_date             [ISODCL (865, 881)]; /* 8.4.26.1 */
        char file_structure_version     [ISODCL (882, 882)]; /* 711 */
        char unused4                    [ISODCL (883, 883)];
        char application_data           [ISODCL (884, 1395)];
        char unused5                    [ISODCL (1396, 2048)];
} ipd;

static struct iso_directory_record {
        unsigned char length            [ISODCL (1, 1)];   /* 711 */
        char ext_attr_length            [ISODCL (2, 2)];   /* 711 */
        char extent                     [ISODCL (3, 10)];  /* 733 */
        char size                       [ISODCL (11, 18)]; /* 733 */
        char date                       [ISODCL (19, 25)]; /* 7 by 711 */
        char flags                      [ISODCL (26, 26)];
        char file_unit_size             [ISODCL (27, 27)]; /* 711 */
        char interleave                 [ISODCL (28, 28)]; /* 711 */
        char volume_sequence_number     [ISODCL (29, 32)]; /* 723 */
        unsigned char name_len          [ISODCL (33, 33)]; /* 711 */
        char name                       [34]; /* Not really, but we need something here */
} idr;


static void add_dirent(char *dir, int *dirlen, int dirsize,
           char *name, int namelen, int extent, int size, int flags, int xa,
           struct tm *ft	// for proper timestamp -- by EG
           )
{
   int reclen;
   unsigned char xa_attr[2];

   reclen = 33 + namelen;
   if(reclen & 1) reclen++;

   if(xa >= 0) reclen += 14; // add XA signature

   if (reclen > (ISO_DIR_SIZE-(*dirlen%ISO_DIR_SIZE)))  // avoid directory record crossing block boundaries
      *dirlen += (ISO_DIR_SIZE-(*dirlen%ISO_DIR_SIZE));

   if (dir)
   {
      if ((*dirlen) + reclen > dirsize)
      {
         fprintf(stderr,"Fatal Error: add_dirent - max dirsize exceeded\n");
         exit(1);
      }

      memset(dir+(*dirlen), 0, reclen);
   }

   memset(&idr, 0 , sizeof(idr));

   idr.length[0] = reclen;
   idr.ext_attr_length[0] = 0;
   set_733(idr.extent,extent);
   set_733(idr.size,size);

//	 __asm { int 3 };
   idr.date[0] = ft->tm_year;		// by EG
   idr.date[1] = ft->tm_mon+1;
   idr.date[2] = ft->tm_mday;
   idr.date[3] = ft->tm_hour;
   idr.date[4] = ft->tm_min;
   idr.date[5] = ft->tm_sec;
   idr.date[6] = gmt_off; /* This is in units of 15 minutes from GMT */

   idr.flags[0] = flags;
   idr.file_unit_size[0] = 0;
   idr.interleave[0] = 0;

   set_723(idr.volume_sequence_number,1);

   idr.name_len[0] = namelen;

   if (dir)
   {
      memcpy(dir+(*dirlen), &idr, 33);
      memcpy(dir+(*dirlen)+33, name, namelen);
   }

   /* XA signature */

   if (xa >= 0)
   {
      if (flags == 2) {
         xa_attr[0] = 0x8d;  // directory
         xa_attr[1] = 0x55;
      } else {
         if (xa == 0) {
            xa_attr[0] = 0x0d;  // form1 file
            xa_attr[1] = 0x55;
         } else {
            xa_attr[0] = 0x15;  // form2 file
            xa_attr[1] = 0x55;
         }
      }
      
      if (dir)
      {
         dir[*dirlen+reclen-10] = xa_attr[0];
         dir[*dirlen+reclen- 9] = xa_attr[1];
         dir[*dirlen+reclen- 8] = 'X';
         dir[*dirlen+reclen- 7] = 'A';
         dir[*dirlen+reclen- 6] = xa;
      }
   }

   (*dirlen) += reclen;
}

static void make_ipd(dir_s **dir_entry)
{
   char iso_time[17];
   int len;
   char name[1];

   memset(&ipd,0,sizeof(ipd));

   ipd.type[0] = 1;
   set_str(ipd.id,"CD001",5);
   ipd.version[0] = 1;

   set_str(ipd.system_id,CD_SYSTEM_ID,32);
   set_str(ipd.volume_id,CD_VOLUME_ID,32);

   set_733(ipd.volume_space_size,ISO_FS_BLOCKS);
   /* we leave ipd.escape_sequences = 0 */

   set_723(ipd.volume_set_size,1);
   set_723(ipd.volume_sequence_number,1);
   set_723(ipd.logical_block_size,2048);

   set_733(ipd.path_table_size,path_table_size);
   set_731(ipd.type_l_path_table,PATH_TABLE_L_EXTENT);
   set_731(ipd.opt_type_l_path_table,0);
   set_732(ipd.type_m_path_table,PATH_TABLE_M_EXTENT);
   set_732(ipd.opt_type_m_path_table,0);

   len = 0;
   name[0] = 0;
   add_dirent(ipd.root_directory_record, &len, 34,
              name, 1, ROOT_DIR_EXTENT, LEN2BLOCKS(dir_entry[0]->len)*ISO_DIR_SIZE, 2, -1,
              &dt	// disc creation time -- EG
              );
   if(len!=34)
   {
      fprintf(stderr,"Internal error in make_ipd\n");
      exit(1);
   }

   set_str(ipd.volume_set_id , CD_VOLUME_SET_ID , 128);
   set_str(ipd.publisher_id  , CD_PUBLISHER_ID  , 128);
   set_str(ipd.preparer_id   , CD_PREPARER_ID   , 128);
   set_str(ipd.application_id, CD_APPLICATION_ID, 128);

   set_str(ipd.copyright_file_id    , " ", 37);
   set_str(ipd.abstract_file_id     , " ", 37);
   set_str(ipd.bibliographic_file_id, " ", 37);

   sprintf(iso_time, "%4.4d%2.2d%2.2d%2.2d%2.2d%2.2d00",
              1900+(dt.tm_year), dt.tm_mon+1, dt.tm_mday,	// EG
              dt.tm_hour, dt.tm_min, dt.tm_sec);
   iso_time[16] = gmt_off; /* This is in binary! Units of 15 minutes from GMT */

   memcpy(ipd.creation_date,    iso_time,17);
   memcpy(ipd.modification_date,iso_time,17);
   memcpy(ipd.expiration_date,  "0000000000000000\0",17);
   memcpy(ipd.effective_date,   iso_time,17);

   ipd.file_structure_version[0] = 1;

   /*  The string CD-XA001 in this exact position indicates  */
   /*  that the disk contains XA sectors.                    */
   memcpy(ipd.application_data+141, "CD-XA001", 8);
}

static void init_iso_dir(char *dir, int *dirlen, int self_dir_len, int parent_dir_len, int self, int parent)
{
   char name[1];

   if (dir) memset(dir,0,self_dir_len);
   *dirlen = 0;

   /* add . and .. entries */

   name[0] = 0;
   add_dirent(dir, dirlen, self_dir_len, name, 1, self,   self_dir_len, 2, 0,
              &dt /* disc creation time -- EG */ );

   name[0] = 1;
   add_dirent(dir, dirlen, self_dir_len, name, 1, parent, parent_dir_len, 2, 0,
              &dt /* disc creation time -- EG */ );
}

void to_dchar(char *dest, char *src, int max)
{
int j;

   for (j = 0; j < (strlen(src) <= max ? strlen(src) : max); j++)
       dest[j] = (isalnum(src[j]) ? toupper(src[j]) : '_'); // d-char (ISO-9660)
   dest[j] = 0;
}

void convert_name(char *dstname, char *srcname, int type, opts_s *opts)
{
char *name, *ext;
char FORM1_EXT[MAX_PATH] = { 0, };
int j;

         if (!opts->ascii && type != DIR)
            to_dchar(FORM2_EXT, FORM2_EXT, 30);

         for (j = strlen(srcname); j > 0; j--)
         {
             if (srcname[j-1] == DIR_SLASH || srcname[j-1] == ':')
                break;
         }
         name = &srcname[j];
/*
         name = (char *) strrchr(srcname, DIR_SLASH);

         if (name == NULL)
            name = srcname;
         else
            name = &name[1];  // skip directory slash
*/
         strcpy(dstname, name);

         if (type != DIR)
         {
            ext = (char *) strrchr(dstname, '.');

            if (ext != NULL)
            {
               if (type == FORM1)
               {
                  strcpy(FORM1_EXT,&ext[1]);
                  if (!opts->ascii) to_dchar(FORM1_EXT, FORM1_EXT, 30);
                  ext[0] = 0; // trim the file extension, if any
               }
               else // FORM2
                  if (!opts->form2_ext_to_name) ext[0] = 0; // trim the file extension, if any
            }
         }

         if (!opts->long_file_names)
         {
            if (strlen(dstname) > 8) dstname[8] = 0;

            if (type != DIR)
            {
               if (strlen(FORM1_EXT) > 3) FORM1_EXT[3] = 0;
               if (strlen(FORM2_EXT) > 3) FORM2_EXT[3] = 0;
            }
         }

         if (!opts->ascii)
            if (type != DIR)
               to_dchar(dstname, dstname, 30-(strlen(type == FORM1 ? FORM1_EXT : FORM2_EXT)));
            else
               to_dchar(dstname, dstname, 31);  // extensions not allowed in dirs on ISO-9660

         if (type != DIR)
         {
            strcat(dstname, "."); // separator is mandatory

            if (type == FORM2)
               strcat(dstname, FORM2_EXT);
            else // FORM1
               if (ext != NULL) strcat(dstname, FORM1_EXT);

            strcat(dstname, ";1"); // file version number
         }

#ifdef _WIN32
         if (opts->ascii && !opts->dos_charset)	// by EMB
					 CharToOem(dstname, dstname); // use DOS character set for international file names
#endif
}

int get_parent(entry_s **entry, int current)
{
int n, number;

    number = 0;

    if (entry[current]->parent == 0) return 0; // root

    for (n=0;n<current;n++)
    {
        if (entry[n]->type == DIR)
        {
           number++;
           if (entry[current]->parent == entry[n]->number) // dir ID and parent ID are equal
              return number;
        }
    }

return -1; // unknown parent
}

int mk_vcd_iso_fs(entry_s **entry, int num_entries, dir_s **dir_entry, int num_dirs, opts_s *opts, int pass)
{
   int i, n, len;
   char filename[MAX_PATH];
   char dirname[MAX_PATH];
   int self_dir_len, parent_dir_len;
   int current_dir, cur_dir_extent;
   int cur_dir_len[MAX_DIRS] = { 0, };
   int parent_dir;
   time_t T;

   /* some initializations */

	 tzset();
   time(&T);
   memcpy( &dt, localtime( &T ), sizeof( struct tm ) );	// make copy of local time -- EG
//	 __asm { int 3 };
	 gmt_off = -_timezone / ( 15 * 60 ) + ( ( dt.tm_isdst > 0 ) ? 4 : 0 );

   /* Allocate memory for directory records on PASS_3 */

   if (pass == PASS_3)
      for (i = 0; i < num_dirs; i++)
          dir_entry[i]->record = (char *) malloc(LEN2BLOCKS(dir_entry[i]->len)*ISO_DIR_SIZE);

   dirname[0] = 0;
   current_dir = 0;
   parent_dir = 0;
   cur_dir_extent = ROOT_DIR_EXTENT;

   if (pass == PASS_1)
      self_dir_len = 0;
   else
      self_dir_len = LEN2BLOCKS(dir_entry[0]->len)*ISO_DIR_SIZE;

   init_iso_dir(dir_entry[current_dir]->record, &cur_dir_len[current_dir], self_dir_len, self_dir_len, cur_dir_extent, cur_dir_extent);

   dir_entry[0]->extent = ROOT_DIR_EXTENT;
   dir_entry[0]->parent = ROOT_DIR_EXTENT;
   dir_entry[0]->parent_num = 1;
   dir_entry[0]->name[0] = 0;

   for(n=0;n<num_entries;n++)
   {
      if (entry[n]->type == DIR) // new dir
      {
         current_dir++;

         parent_dir = get_parent(entry, n);

         if (parent_dir == -1)
         {
         fprintf(stderr,"Error: unknown directory specified as parent: %s", entry[n]->name);
         exit(1);
         }

         if (pass == PASS_1) {
            self_dir_len = 0;
            parent_dir_len = 0;
         } else {
            self_dir_len = LEN2BLOCKS(dir_entry[current_dir]->len)*ISO_DIR_SIZE;
            parent_dir_len = LEN2BLOCKS(dir_entry[parent_dir]->len)*ISO_DIR_SIZE;
         }

         convert_name(dirname, entry[n]->name, entry[n]->type, opts);
         strcpy(dir_entry[current_dir]->name, dirname);
         if (pass == PASS_3) printf("%s\\\n",dirname);

         if (pass != PASS_1) cur_dir_extent += (LEN2BLOCKS(dir_entry[current_dir-1]->len));

         dir_entry[current_dir]->extent = cur_dir_extent;
         dir_entry[current_dir]->parent = dir_entry[parent_dir]->extent;
         dir_entry[current_dir]->parent_num = parent_dir+1;

         /* Initialize directory */

         init_iso_dir(dir_entry[current_dir]->record, &cur_dir_len[current_dir], self_dir_len, parent_dir_len, cur_dir_extent, dir_entry[current_dir]->parent);

         /* Add this directory to parent directory */

         add_dirent(dir_entry[parent_dir]->record, &cur_dir_len[parent_dir], parent_dir_len,
                   dirname, strlen(dirname), cur_dir_extent, self_dir_len, 2, 0,
                   &dt /* disc creation time -- EG */ );
      }
      else if (entry[n]->type != NOTFILE) // files
      {
         convert_name(filename, entry[n]->name, entry[n]->type, opts);
         if (pass == PASS_3) printf("%s\\%s\n",dirname,filename);

         if (entry[n]->type == FORM2)
            len = entry[n]->size*2048; // dummy size for FORM2 files
         else
            len = entry[n]->length;

         add_dirent(dir_entry[current_dir]->record, &cur_dir_len[current_dir], self_dir_len,
                   filename, strlen(filename), entry[n]->extent, len, 0, entry[n]->number,
                   &entry[n]->ft /* file last modification time -- EG */);
      }
   }

   cur_dir_extent += (LEN2BLOCKS(cur_dir_len[current_dir]));

   if (pass == PASS_1)
      for (i = 0; i < current_dir+1; i++)
          dir_entry[i]->len = cur_dir_len[i]; // fill dir_entry[]->len with directory sizes

   if (pass == PASS_3) // do not output data until PASS_3
   {
      /* Output boot record */

      for(i=0;i<PVD_EXTENT;i++) output_form1(i,zero2048);

      /* Output PVD, EVD & Path Tables */

      make_path_tables(dir_entry, num_dirs);
      make_ipd(dir_entry);

      memset(buff2048, 0, 2048);

      buff2048[0] = 0xff;
      buff2048[1] = 'C';
      buff2048[2] = 'D';
      buff2048[3] = '0';
      buff2048[4] = '0';
      buff2048[5] = '1';
      buff2048[6] = 0x01;

      output_form1(PVD_EXTENT,(char *) &ipd);
      output_form1(EVD_EXTENT, buff2048);
      output_form1(PATH_TABLE_L_EXTENT,path_table_l);
      output_form1(PATH_TABLE_M_EXTENT,path_table_m);

      /* Output directory entries */

      for(i = 0; i < num_dirs; i++)
         for(n = 0; n < (LEN2BLOCKS(dir_entry[i]->len)); n++)
            output_form1(dir_entry[i]->extent+n, dir_entry[i]->record+(n*ISO_DIR_SIZE));

      /* Fill the rest of the ISO FS with empty sectors */

      for(i=cur_dir_extent;i<START_FILE_EXTENT;i++)
         output_form1(i,zero2048);

   } // end: if (pass == PASS_3)

   return cur_dir_extent;
}

