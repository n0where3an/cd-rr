/*

    2012/11/02 - 1.5.1e - Elicit Gingers

    mkvcdfs.c: some function prototypes included to avoid stupid warning

    2007/08/04 - 1.5.1d - Elicit Gingers

    mkvcdfs.c: preserve original timestamps of files placed on disc

    2006/01/30 - 1.5.1c - Elicit Gingers

    mkvcdfs.c: default volume label changed

    2005/05/18 - 1.5.1b - Elic MegaBugger

    mkvcdfs.c: fixed OEM filenames processing
    mkvcdfs.c: filenames only from paramfile

    2005/04/10 - 1.5.1a - Elic MegaBugger

    mkvcdfs.c: fixed minor bug when paramfile is not exist

    2003/03/14 - 1.5.1 - Alex Noe & DeXT
    
    mkvcdfs.c: added CloneCD (CCD/SUB/IMG) image output support (Alex Noe)
    mkvcdfs.c: added filelist input support, as well as parameter file (Alex Noe)
    mkvcdfs.c: cuesheets are only written after the image is created
    mkvcdfs.c: enhanced file buffering

    2002/10/17 - 1.5 - DeXT
    
    mkvcdfs.c: added -s option (creates single track image, removes
               minimum file size limit, removes pregaps & separate ISO
               Bridge track, better use of available space)
    mkvcdfs.c: new command line options parser, options are more user friendly
    mkvcdfs.c: by default all file names are now kept as ASCII (-l and -a
               options removed), added -isolevel1 and -isolevel2 options
    mkvcdfs.c: better memory management: buffers are now allocated at run-time
    mkvcdfs.c: added 2 empty blocks at the end of ISO track to avoid potential
               reading problems when no Form2 files where added
    mkvcdfs.c: show actual data written to the CD along with estimated CD size

    2002/07/21 - 1.4.2 - Martin Lück & DeXT

    mkvcdfs.c: added ML's wildcard code (for MSVC only)
    mkvcdfs.c: added -c option (use DOS charset for input file names)

    2002/06/24 - 1.4.1 - DeXT

    mkvcdfs.c: fixed incorrect track numbering in CUE file

    2002/06/20 - 1.4 - DeXT
    
    mkvcdfs.c: new option: -x (keep form2 file extension)
    mkvcdfs.c: new file collect routine, to allow subdirs for form2 files
    mkvcdfs.c: default screen output is now stdout (can be redirected)
    mkvcdfs.c: fixed bug where input files were not closed
    mkvcdfs.c: BIN image is now referenced as relative in TOC, CUE files
    mkvcdfs.c: big speedup thanks to new r/w buffering

    2002/05/19 - 1.3 - DeXT
    
    mkvcdfs.c: new options: -a, -d

    2002/05/19 - 1.2 - DeXT
    
    mkvcdfs.c: command line parser is now less tolerant to bad syntax
    mkvcdfs.c: ISO FS is now built before copying actual file data
    mkvcdfs.c: show total estimated CD size
    mkvcdfs.c: new XA subheader which is more compatible with DVD players

    2002/05/06 - 1.2pre2 - DeXT
    
    mkvcdfs.c: added progress counter and speed indicator
    mkvcdfs.c: fixed DAT filesize (was 1 sector higher w/multiple of 2324)
    mkvcdfs.c: fixed CUE file not being deleted on failure
    mkvcdfs.c: XA subheader now marks EOF
    
    2002/05/04 - 1.2pre - Nic & DeXT
    
    mkvcdfs.c: new command line option parser (Nic)
    mkvcdfs.c: new features (set volume name, M2F2 ext, output image) (Nic)
    mkvcdfs.c: changed default volume name and image base name
    mkvcdfs.c: added simple d-char (ISO-9660) converter

    2002/04/18 - beta 1.1 - DeXT
    
    mkvcdfs.c: added CUE output support

    2002/04/17 - beta 1.0 - DeXT

    mkvcdfs.c: changed output file method to fopen() (open() worked in text
      mode, perhaps a MinGW bug?)
    mkvcdfs.c: removed 30+45 empty sector padding in M2F2 track
    mkvcdfs.c: now will accept any file (not just MPEG)
    mkvcdfs.c: changed CD/XA track subheader to Form2/Data
    mkvcdfs.c: fixed DAT file length and track file length:
      * DAT file now has right size (missed 1 sector)
      * CD/XA track now has 2 "ghost" sectors at the end, to
        prevent any potential TAO problems


    -- Original notes --


    mkvcdfs: make a VCD image from several MPEG-1 files suitable
             for burning to CDR with cdrdao

    Usage:

      mkvcdfs mpegfile1 mpegfile2 ....

    mkvcdfs creates 2 files:

    vcd.toc          contains the table of contents of the VCD
    vcd_image.bin    contains the CD-Image itself


    Copyright (C) 2000 Rainer Johanni <Rainer@Johanni.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#ifdef _WIN32
#include <windows.h>
#ifdef _MSC_VER
#include <io.h>
#endif
#endif
#include <stdio.h>
#include <fcntl.h>
#include <string.h>
#include <ctype.h>
#include <sys/stat.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include "defaults.h"
#include "ecc.h"
#include "buffer.h"
#include "raw96.h"
#include "funcs.h"

#define EOF_INDICATOR 0xffffffff

static unsigned char data[2324];
static unsigned char outrec[2352];
static int maxrec = 0;

struct buffer_s read_buffer;
struct buffer_s bin_file_buffer;
struct buffer_s sub_file_buffer;
struct entry_s *entry[MAX_FILES];
struct dir_s *dir_entry[MAX_DIRS];
struct opts_s opts;

int num_entries;
int num_dirs;
int tno = 1, tindex = 1, track_start = 0;

// Extern variables set here
char CD_VOLUME_ID[33];
char OUTPUT_BASE_NAME[MAX_PATH];
char BINARY_OUTPUT_FILE[MAX_PATH];
char SUB_OUTPUT_FILE[MAX_PATH];
char CCD_FILE[MAX_PATH];
char TOC_FILE[MAX_PATH];
char CUE_FILE[MAX_PATH];
char FORM2_EXT[MAX_PATH];
int ISO_FS_BLOCKS = 300;
int START_FILE_EXTENT = 50;

#ifdef _WIN32
   LARGE_INTEGER Frequency, PerformanceCount, last_counter;   // counter stuff
#endif

enum { COMMAND_BAD, COMMAND_SET_VOLUME, COMMAND_SET_OUTPUTNAME, \
		COMMAND_ADD_FORM1_FILES, COMMAND_ADD_MOVIE_FILES, \
		COMMAND_DEFAULT_EXT, COMMAND_ADD_DIR, COMMAND_ADD_LIST_FILES };

static void write_record(int rec)
{
   int ret;
   SUBBLOCK subb;

   if (tindex > 0)
      do_encode_sub_pw(&subb,tno,tindex,rec - track_start,rec);
   else // pregap
      do_encode_sub_pw(&subb,tno,tindex,PREGAP_SIZE - (rec - (track_start - PREGAP_SIZE)),rec);

   switch (opts.imgformat) {
      case BIN:
      case CCD:
                BufWriteSeek(rec*2352, &bin_file_buffer);
                break;
      case RAW:
                BufWriteSeek(rec*2448, &bin_file_buffer);
                break;
   }

   ret = BufWrite(outrec, 2352, &bin_file_buffer);

   switch (opts.imgformat) {
      case CCD:
                BufWriteSeek(rec*96, &sub_file_buffer);
                BufWrite((char *)&subb, 96, &sub_file_buffer);
                break;
      case RAW:
                BufWrite((char *)&subb, 96, &bin_file_buffer);
                break;
   }

   if (!ret)
   {
      fprintf(stderr,"Error writing to binary output file\n");
      perror("write");
      exit(1);
   }
}

static void output_zero(int rec)
{
   /* Output a zero record */

   do_encode_L2(outrec, MODE_0, rec+150);
   write_record(rec);
}

void output_form1(int rec, unsigned char *data)
{
   int i;

   /* Output a CDROM XA Mode 2 Form 1 record,
      we fill gaps with zero records */

   if(rec+1>maxrec)
   {
      for(i=maxrec;i<rec;i++) output_zero(i);
      maxrec = rec+1;
   }

   /* We use the same subheader for all form 1 sectors,
      I dont know if this is completly correct */

   outrec[16] = outrec[20] = 0;
   outrec[17] = outrec[21] = 0;
   outrec[18] = outrec[22] = 8;
   outrec[19] = outrec[23] = 0;

   /* Copy the data to outrec */

   memcpy(outrec+24,data,2048);

   /* Adding of sync, header, ECC, EDC fields */

   do_encode_L2(outrec, MODE_2_FORM_1, rec+150);

   /* Output record */

   write_record(rec);
}

void output_form2(int rec, int h1, int h2, int h3, int h4, unsigned char *data)
{
   int i;

   /* Output a CDROM XA Mode 2 Form 1 record,
      we fill gaps with zero records */

   if(rec+1>maxrec)
   {
      for(i=maxrec;i<rec;i++) output_zero(i);
      maxrec = rec+1;
   }

   /* Subheader */

   outrec[16] = outrec[20] = h1;
   outrec[17] = outrec[21] = h2;
   outrec[18] = outrec[22] = h3;
   outrec[19] = outrec[23] = h4;

   /* Copy the data to outrec */

   memcpy(outrec+24,data,2324);

   /* Adding of sync, header, EDC */

   do_encode_L2(outrec, MODE_2_FORM_2, rec+150);

   /* Output record */

   write_record(rec);
}

int copy_file(entry_s *entry)
{
   FILE *fd;
   int tag;
   unsigned int progress, speed = 0;
   int i, fn, cn, sm, ci, extent;
   int blocksize;

      extent = entry->extent;
      
      if (entry->type == FORM2)
         blocksize = 2324;
      else // FORM1
         blocksize = 2048;
      
      printf("Copying \"%s\"\n", entry->name);

      fd = fopen(entry->name,"rb");
      if (fd==0)
      {
         fprintf(stderr,"Can not open file %s\n", entry->name);
         perror("open");
         return (-1);
      }

      if (!BufInit(fd, READ_BUF_SIZE, &read_buffer))
      {
         fprintf(stderr,"Not enough free memory for buffers!");
         exit(1);
      }

      /* Output the file itself */

      tag = 0; /* new file starts */

      for(i=0;i<entry->size;i++)
      {

#define SHOW_INTERVAL 150
#define SPEED_INTERVAL (SHOW_INTERVAL*6)

         if (!(i%SHOW_INTERVAL))
         {
#ifdef _WIN32
            if (i > 0 && !(i%(SPEED_INTERVAL))) { 
            QueryPerformanceCounter(&PerformanceCount);
            speed = (unsigned int)(SPEED_INTERVAL*blocksize)/(1+(PerformanceCount.QuadPart - last_counter.QuadPart)/(1+(Frequency.QuadPart/1000)));
            last_counter = PerformanceCount;
            }
#endif
            progress = i*100/entry->size;
            fprintf(stderr,
                   "\r[Progress: %02u%%  Speed: %u KB/s]  ",
                   progress, speed);
         }

//         readed = fread(data,1,blocksize,fd);
         BufRead (data, blocksize, &read_buffer, entry->length);
         
         if (i == entry->size-1) // if last sector
         {
            tag = EOF_INDICATOR;
//            if (readed < blocksize)
//               memset(data+readed, 0, blocksize-readed); // fill remaining with 0s
            if (entry->length%blocksize != 0)
               memset(data + (entry->length%blocksize), 0, blocksize - (entry->length%blocksize)); // fill remaining with 0s
         }

         if(!opts.single_track && entry->type == FORM2 && tag == EOF_INDICATOR && i < (300 - PREGAP_SIZE))  // minimum track size
         {
            fprintf(stderr,"Not enough data\n");
            fclose(fd);
            return (-1);
         }

/*
   XA subheader format: FileNumber, ChannelNumber, SubMode, CodingInformation

   FileNumber = the unique ID for a single XA file
   ChannelNumber = the file is multimedia (1) or not (0)
   SubMode = XA content description: struct SubMode { uint8 EndOfRecord:1;
   CodingInformation = type of data:                  uint8 VideoData:1;
         0x00 = empty/data sector                     uint8 ADCPM:1;
         0x0f = video sector                          uint8 Data:1;
         0x7f = audio sector                          uint8 TriggerOn:1;
         0x80 = MPEG2 (SVCD)                          uint8 Form2:1;
                                                      uint8 RealTime:1;
                                                      uint8 EndOfFile:1; };
*/

         if (entry->type == FORM2)
         {
            fn = entry->number;
            cn = 1;
            sm = 0x68; // Form2, Data
            ci = 0x80;

            if (tag==EOF_INDICATOR)
            {
               sm |= 0x01; // End of record
               sm |= 0x80; // End of file
            }

            output_form2(extent++,fn,cn,sm,ci,data);
         }   
         else // FORM1
         {
            output_form1(extent++,data);
         }

         if(tag==EOF_INDICATOR)
         {
            fprintf(stderr,
                   "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                   "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                   "                                                          "
                   "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b"
                   "\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b\b");

//            printf("Done, got %d sectors\n",i+1);
            break;
         }
      }
      
      BufFree(&read_buffer);

      fclose(fd);
      
      return extent;
}

void sort_dirs(entry_s **entry, int num_entries)
{
int n;
int current_parent;
int copied, copied_pos;
struct entry_s *entry_copy[MAX_FILES];

    current_parent = 0;
    copied = 0;
    copied_pos = -1;

    while (copied < num_entries)
    {
        for (n = 0; n < num_entries; n++)
        {
            if (entry[n]->parent == current_parent)
            {
                entry_copy[copied] = entry[n];
                copied++;
            }
        }

        do { copied_pos++; } while (copied_pos < num_entries && entry_copy[copied_pos]->type != DIR);

        if (copied_pos < num_entries) current_parent = entry_copy[copied_pos]->number;
    }

    for (n = 0; n < num_entries; n++)
        entry[n] = entry_copy[n];
}

int find_parent(entry_s **entry, int current)
{
char *dirname;
int name_size, n;

         dirname = (char *) strrchr(entry[current]->name, DIR_SLASH);

         if (dirname == NULL) return 0; // no parent (lies on root)

         name_size = dirname - entry[current]->name;

         for (n=0;n<current;n++)
         {
             if (entry[n]->type == DIR)
             {
                   if (strlen(entry[n]->name) == name_size &&
                      !strncasecmp(entry[current]->name, entry[n]->name, name_size))
                      return entry[n]->number;
             }
         }

return -1; // unknown parent
}

int remove_newline(char *name)
{
    int i, len;
    
    len = strlen(name);
    
    if (!len) return 0;
    
    for (i = 0; i < len; i++)
    {
        if (name[i] == 0x0d || name[i] == 0x0a)
           name[i] = 0;
    }

    return 1;
}

#ifdef _MSC_VER
int add_files(char* pFirstFile, int command) // written by Martin Lück
{	
	int		bResult		= 0;
	int		lNext		= 0;
	struct _finddata_t fileinfo;
	long	hFile		= 0;
	char*	pPath		= 0;
	char*	pFileName	= 0;

	/* Get path...a backslash indicates a path */
	char* pDest = strrchr(pFirstFile, '\\');

	if (pDest)
	{
		pPath = malloc(pDest-pFirstFile+1);
		memset(pPath,0, pDest-pFirstFile+1);
		strncpy(pPath, pFirstFile, pDest-pFirstFile);
		
		/* add path as new dir */
//		entry[num_entries] = (entry_s *) malloc (sizeof(entry_s));
//		entry[num_entries]->name = pPath;
//		entry[num_entries]->type = DIR;
//		num_entries++;
//		num_dirs++;
	}

	/* Search the files which match the search pattern */
	hFile = _findfirst(pFirstFile, &fileinfo);
	if (hFile == -1)
		return bResult;

	do
	{
		/* ignore all directories */
		if (fileinfo.attrib != _A_SUBDIR)
		{
			if (pPath)
			{
				pFileName = malloc(strlen(pPath)+1+strlen(fileinfo.name)+1);
				strcpy(pFileName, pPath);
				strcat(pFileName, "\\");
				strcat(pFileName, fileinfo.name);
			}
			else
			{
				pFileName = malloc(strlen(fileinfo.name)+1);
				strcpy(pFileName, fileinfo.name);
			}
			
			if (command == COMMAND_ADD_FORM1_FILES)
			{
				/* Add form1-files */
				entry[num_entries] = (entry_s *) malloc(sizeof(entry_s));
				entry[num_entries]->name = pFileName;
				entry[num_entries]->type = FORM1;
				num_entries++;
				bResult = 1;
			}
			else if (command == COMMAND_ADD_MOVIE_FILES)
			{
				/* Add form2-files */
				entry[num_entries] = (entry_s *) malloc(sizeof(entry_s));
				entry[num_entries]->name = pFileName;
				entry[num_entries]->type = FORM2;
				num_entries++;
				bResult = 1;
			}

		}
		lNext = _findnext(hFile, &fileinfo);
		
	} while (lNext == 0);

	_findclose(hFile);

	return bResult;
}
#endif

int command = COMMAND_BAD;	// ??? COMMAND_ADD_MOVIE_FILES;  // default command

int parse_params(int argc, char **argv, int idepth) // written by Nic
{
	int i;
	FILE *lf;
	FILE *pf;
	char filename[MAX_PATH];
	char **next_command;
	
	// Set Defaults First
	if (!idepth)
	{
	strcpy(CD_VOLUME_ID, "RR_XCD");	// EG
	strcpy(OUTPUT_BASE_NAME, "image");
	strcpy(FORM2_EXT, "dat");
	opts.long_file_names = 1;
	opts.ascii = 1;
	opts.dos_charset = 0;
	opts.form2_ext_to_name = 0;
	opts.single_track = 0;
	opts.imgformat = BIN;
	num_entries = 0;
	num_dirs = 1; // root directory
	}

#define OPT(a) !strcasecmp(argv[i],a)

	// Loop thru params
	for ( i = 1; i != argc; i ++ )
	{
		if ( argv[i][0] == '-' )	// if argv[i] is a command
		{
			int proper_command;	// by EMB
	    if      (OPT("-?") || OPT("-h") || OPT("-help")) { return 0; }
			if ( idepth > 0 ) {
				proper_command = 1;	// by EMB
		    if (OPT("-v") || OPT("-volume"))  { command = COMMAND_SET_VOLUME; }
		    else if (OPT("-o") || OPT("-output"))  { command = COMMAND_SET_OUTPUTNAME; }
		    else if (OPT("-f") || OPT("-file"))    { command = COMMAND_ADD_FORM1_FILES; }
		    else if (OPT("-m") || OPT("-movie"))   { command = COMMAND_ADD_MOVIE_FILES; }
		    else if (OPT("-list"))                 { command = COMMAND_ADD_LIST_FILES; }
		    else if (OPT("-e") || OPT("-ext"))     { command = COMMAND_DEFAULT_EXT; }
		    else if (OPT("-d") || OPT("-dir"))     { command = COMMAND_ADD_DIR; }
		    else if (OPT("-isol1") || OPT("-isolevel1")) { opts.ascii = 0; opts.long_file_names = 0; command = COMMAND_BAD; }
		    else if (OPT("-isol2") || OPT("-isolevel2")) { opts.ascii = 0; opts.long_file_names = 1; command = COMMAND_BAD; }
		    else if (OPT("-c") || OPT("-dos"))     {
#ifdef _WIN32
					SetFileApisToOEM();	// by EMB
#endif
					opts.dos_charset = 1; command = COMMAND_BAD;
				}

		    else if (OPT("-x") || OPT("-extname")) { opts.form2_ext_to_name = 1; command = COMMAND_BAD; }
		    else if (OPT("-s") || OPT("-single"))  { opts.single_track = 1; command = COMMAND_BAD; }
		    else if (OPT("-ccd") || OPT("-clonecd")) { opts.imgformat = CCD; command = COMMAND_BAD; }
		    else if (OPT("-raw") || OPT("-raw_pw")) { opts.imgformat = RAW; command = COMMAND_BAD; }
/* old commands: maintained for compatibility */
		    else if (OPT("-l") || OPT("-long"))    { opts.long_file_names = 1; command = COMMAND_BAD; }
		    else if (OPT("-a") || OPT("-ascii"))   { opts.ascii = 1; command = COMMAND_BAD; }
				else proper_command = 0;
/* end old commands */
			} else proper_command = 0;
			if ( !proper_command && /*else if (*/ OPT("-paramfile")) // by Alex Noe	// by EMB
			{
				proper_command = 1;
				pf=fopen(argv[i+1],"rt");
				if ( pf == NULL ) {	// by EMB
					fprintf( stderr, "ERROR: cannot open paramfile %s - %s!\n",
						argv[i+1], strerror( errno ) );
					return 0;	// FAIL
				}
				(next_command)=(char**)malloc(8);
				next_command[0]=NULL;
				next_command[1]=(char*)malloc(_MAX_PATH);
				while (fgets(next_command[1],_MAX_PATH,pf))
				{
					next_command[1][lstrlen(next_command[1])-1]=0;
					if ( !parse_params(2,next_command,idepth+1) )
						return 0;
				}
				fclose(pf);
				free(next_command[1]);
				free(next_command);
				i++;
			}
			/*else*/ if ( !proper_command )
				{ return 0; } // unknown command
		}
		else	// if not a command then a parameter for a command
		{
			if ( idepth == 0 )	// by EMB
				return 0;
			switch ( command )
			{
			case COMMAND_BAD:	// unknown command
				return 0;		// FAIL
				break;
			case COMMAND_SET_VOLUME:
				to_dchar(CD_VOLUME_ID, argv[i], 32);
				command = COMMAND_BAD; // next argument has to be a command, not a parameter
				break;
			case COMMAND_SET_OUTPUTNAME:
				strcpy(OUTPUT_BASE_NAME, argv[i]);
				command = COMMAND_BAD; // next argument has to be a command, not a parameter
				break;
			case COMMAND_ADD_FORM1_FILES:
				if ( num_entries >= MAX_FILES )
				{
					fprintf(stderr, "ERROR: Maximum of %d files exceeded!\n",MAX_FILES);
					return 0;	// FAIL
				}
				else
#ifdef _MSC_VER
				if ( idepth == 0 )	// by EMB
					add_files(argv[i], command); // MSVC does not expand wilcards
				else
#endif
				{
					entry[num_entries] = (entry_s *) malloc(sizeof(entry_s));
//					entry[num_entries]->name = argv[i];
					entry[num_entries]->name = (char*)malloc(strlen(argv[i])+1);
					strcpy(entry[num_entries]->name,argv[i]);
					entry[num_entries]->type = FORM1;
					num_entries++;
				}
				break;
			case COMMAND_ADD_MOVIE_FILES:
				if ( num_entries >= MAX_FILES )
				{
					fprintf(stderr, "ERROR: Maximum of %d files exceeded!\n",MAX_FILES);
					return 0;	// FAIL
				}
				else
#ifdef _MSC_VER
				if ( idepth == 0 )	// by EMB
					add_files(argv[i], command); // MSVC does not expand wilcards
				else
#endif
				{
					entry[num_entries] = (entry_s *) malloc(sizeof(entry_s));
//					entry[num_entries]->name = argv[i];
					entry[num_entries]->name = (char*)malloc(strlen(argv[i])+1);
					strcpy(entry[num_entries]->name,argv[i]);
					entry[num_entries]->type = FORM2;
					num_entries++;
				}
				break;
			case COMMAND_ADD_LIST_FILES:
				if ( num_entries >= MAX_FILES )
				{
					fprintf(stderr, "ERROR: Maximum of %d files exceeded!\n",MAX_FILES);
					return 0;	// FAIL
				}
				else
				{
				    lf = fopen(argv[i],"rt");
			        if (!lf)
			        {
			           fprintf(stderr,"Can not open %s\n",argv[i]);
			           perror("open");
			           exit(1);
                    }

                    while(fgets(filename, MAX_PATH, lf))
                    {
						remove_newline(filename); // remove newline char
#ifdef _MSC_VER
						if ( idepth == 0 )	// by EMB
							add_files(filename, COMMAND_ADD_FORM1_FILES); // MSVC does not expand wilcards
						else
#endif
						{
						entry[num_entries] = (entry_s *) malloc(sizeof(entry_s));
						entry[num_entries]->name = (char *) malloc(strlen(filename)+1);
						strcpy(entry[num_entries]->name, filename);
						entry[num_entries]->type = FORM1;
						num_entries++;
						}
                    }
                    fclose(lf);
				}
				break;
			case COMMAND_DEFAULT_EXT:
				strcpy(FORM2_EXT, argv[i]);
				command = COMMAND_BAD; // next argument has to be a command, not a parameter
				break;
			case COMMAND_ADD_DIR:
				if ( num_entries >= MAX_FILES || num_dirs >= MAX_DIRS)
				{
					fprintf(stderr, "ERROR: Maximum of %d dirs exceeded!\n",MAX_DIRS);
					return 0;	// FAIL
				}
				else
				{
					entry[num_entries] = (entry_s *) malloc(sizeof(entry_s));
//					entry[num_entries]->name = argv[i];
					entry[num_entries]->name = (char*)malloc(strlen(argv[i])+1);
					strcpy(entry[num_entries]->name,argv[i]);
					entry[num_entries]->type = DIR;
					entry[num_entries]->number = num_dirs; // unique ID, used for sorting (starts at 1)
					num_entries++;
					num_dirs++;
					command = COMMAND_BAD; // next argument has to be a command, not a parameter
				}
				break;
			}
		}
	}

	// Check if any input files have been entered

	if ((!num_entries)&&(!idepth))
	{
		fprintf(stderr, "ERROR: No input files have been entered!\n");
		return 0;	// FAIL
	}
	else
		return 1;	// OK
}

void output_usage()
{
	printf("usage:\n");
//	printf("mode2cdmaker -m \"movie1.avi\" -d dirname -f \"readme.txt\" -v MODE2CD -e DAT\n\n");
	printf("  m2cdm -paramfile <parameter-file>\nor\n");	// by EMB
	printf("  m2cdm -h, m2cdm -help    this help\n");
	printf( "\nCommands in paramfile (one per line, command parameter in separate line):\n" );
	printf("-v, -volume  volume name (default: RR_XCD)\n");	// by EG
	printf("-o, -output  output image base name (*.bin, *.toc, *.cue) (default: image)\n");
	printf("-f, -file    add Form1 files\n");
	printf("-m, -movie   add movie (Form2) files (by default)\n");
	printf("-d, -dir     add subdirectory\n");
	printf("-e, -ext     set default extension for Form2 files (default: DAT)\n");
//	printf("-l, -long    use long file names (ISO-9660 Level 2)\n");
//	printf("-a, -ascii   use ASCII character set\n");
	printf("-c, -dos     assume DOS charset for input file names\n");
	printf("-x, -extname keep original Form2 file extension\n");
	printf("-s, -single  create a single track image\n");
	printf("-ccd         output CloneCD image (*.img, *.sub, *.ccd)\n");
	printf("-raw         output RAW-96 image (*.img)\n");
	printf("-list        use filelist for input Form1 files\n");
	printf("-paramfile   use parameter file for input\n");
	printf("-isolevel1   force ISO 9660 Level 1 for file names\n");
	printf("-isolevel2   force ISO 9660 Level 2 for file names\n");
}

int main (int argc, char **argv)
{
   FILE *fd;
   FILE *xa_fd;
   FILE *sub_fd = NULL;
   FILE *fd_toc = NULL;
   FILE *fd_cue = NULL;
   FILE *fd_ccd = NULL;
   int i, n;
   int form1_extent, form2_extent, last_extent, extent;
   int form2_file_number, parent, num_tracks;
   struct stat statbuf;
   char *binfilename;

#ifdef _WIN32
	char ansiname[_MAX_PATH];
   QueryPerformanceFrequency(&Frequency);     // Init counter
   QueryPerformanceCounter(&last_counter);
#endif

   printf("m2cdm version " APP_VERSION "  " __DATE__
		 "\n(L) Vinnitsa, exSU, 2003-2012, by redtigra aka -= Elicit Gingers =-"
		 "\nbased on DeXT's mode2cdmaker 1.5.1\n\n" );	// by EMB

   /* Parse the parameters & fill in the appropriate variables */

//	 DebugBreak();
   if ( argc < 2 || !parse_params(argc, argv, 0) )
   {
	   output_usage();
	   exit(1);
   }

   strcpy(BINARY_OUTPUT_FILE, OUTPUT_BASE_NAME);
   strcpy(SUB_OUTPUT_FILE, OUTPUT_BASE_NAME);
   strcpy(CCD_FILE, OUTPUT_BASE_NAME);
   strcpy(TOC_FILE, OUTPUT_BASE_NAME);
   strcpy(CUE_FILE, OUTPUT_BASE_NAME);

   if (opts.imgformat == BIN)
      strcat(BINARY_OUTPUT_FILE, ".bin");
   else // CCD or RAW
      strcat(BINARY_OUTPUT_FILE, ".img");
   strcat(SUB_OUTPUT_FILE, ".sub");
   strcat(CCD_FILE, ".ccd");
   strcat(TOC_FILE, ".toc");
   strcat(CUE_FILE, ".cue");

   /* Open binary output file */

   if (opts.imgformat == RAW && !opts.single_track)
   {
      printf("RAW output only supported for single track!\n");
      exit(1);
   }

   binfilename = (char *) strrchr(BINARY_OUTPUT_FILE, DIR_SLASH);
   if (binfilename == NULL)
      binfilename = BINARY_OUTPUT_FILE;
   else
      binfilename = &binfilename[1]; // skip slash
#ifdef _WIN32
	if ( opts.dos_charset ) {	// cue/toc needs ANSI chars in binfilename	// by EMB
		OemToChar( binfilename, ansiname );
		binfilename = ansiname;
	}
#endif


   printf("Creating \"%s\"\n", BINARY_OUTPUT_FILE);

   xa_fd = fopen(BINARY_OUTPUT_FILE,"wb");
   if(!xa_fd)
   {
      fprintf(stderr,"Can not open %s\n",BINARY_OUTPUT_FILE);
      perror("open");
      exit(1);
   }

   if (opts.imgformat == CCD)
   {
	   sub_fd = fopen(SUB_OUTPUT_FILE,"wb");
	   if(!sub_fd)
	   {
		  fprintf(stderr,"Cannot open %s\n",SUB_OUTPUT_FILE);
		  perror("open");
	      exit(1);
	   }
   }

   /* Initialize write buffers */

   if (!BufInit(xa_fd, WRITE_BUF_SIZE, &bin_file_buffer))
   {
      fprintf(stderr,"Not enough free memory for buffers!");
      exit(1);
   }
   
   if (opts.imgformat == CCD)
   {
      if (!BufInit(sub_fd, WRITE_BUF_SIZE, &sub_file_buffer))
      {
         fprintf(stderr,"Not enough free memory for buffers!");
         exit(1);
      }
   }

   /* Convert character set from DOS if needed */
/*
#ifdef _WIN32
   if (opts.dos_charset)
      for(n=0;n<num_entries;n++)
         OemToChar(entry[n]->name, entry[n]->name); // get ANSI names from DOS file names
#endif
*/

   /* Sort input dirs */

// This assigns every directory entry with a parent dir ID, and all following files
// with the same parent ID in order to simplify the directory sorting routine below.
// Also all the first-level files get parent ID=0 so they are sorted first.

   parent = 0;

   for(n=0;n<num_entries;n++)
   {
      if (entry[n]->type == DIR)
         parent = find_parent(entry, n);
      if (parent == -1)
         { printf("Error: entered subdir has no parent: \"%s\"", entry[n]->name); exit(1); }
      entry[n]->parent = parent;
   }

   sort_dirs(entry, num_entries);

   /* Collect files */

   printf("Collecting file data...\n");

   form1_extent = 0; // relative
   form2_extent = 0; // relative
   form2_file_number = 0;

   for(n=0;n<num_entries;n++)
   {
      if (entry[n]->type == DIR) continue; // skip dirs

      fd = fopen(entry[n]->name,"rb");
      if ( fd == NULL )
      {
         if (stat(entry[n]->name, &statbuf) == 0)
         {
            entry[n]->type = NOTFILE; // entry is not a file (skip when building ISO FS)
            continue;
         }
      else
         {   
            fprintf(stderr,"Can not open file \"%s\"\n",entry[n]->name);
            perror("open");
abort:
            fprintf(stderr,"Fatal Error --- exiting\n");
            fclose(xa_fd);
            remove(BINARY_OUTPUT_FILE);
            if (sub_fd) {
               fclose(sub_fd);
               remove(SUB_OUTPUT_FILE);
            }
            exit(1);
         }
      }

// get info (modification time) about file - by EG
			if ( stat( entry[n]->name, &statbuf ) != 0 ) {
            fprintf(stderr,"Can not get info about file \"%s\"\n",entry[n]->name);
            perror("stat");
				goto abort;
			}
			memcpy( &entry[n]->ft, localtime( &statbuf.st_mtime ), sizeof( struct tm ) );

      fseek(fd, 0L, SEEK_END);
      entry[n]->length = ftell(fd);
      fseek(fd, 0L, SEEK_SET);

      if (entry[n]->type == FORM2)
      {
         if (!opts.single_track)
            form2_extent += PREGAP_SIZE; // pregap
         entry[n]->extent = form2_extent;
         entry[n]->size = (entry[n]->length+2323)/2324; // in FORM2 blocks
         form2_extent += entry[n]->size;
         form2_extent += 2; // postgap
         entry[n]->number = ++form2_file_number;  // useful for TOC file below
      }
      else if (entry[n]->type == FORM1)
      {
         entry[n]->extent = form1_extent;
         entry[n]->size = LEN2BLOCKS(entry[n]->length);
         form1_extent += entry[n]->size;
         entry[n]->number = 0;
      }

      fclose(fd);
   }

/*
   {
   int total_size = 0;
   
   for (n=0;n<num_entries;n++)
   {
       switch(entry[n]->type)
       {
           case FORM1:
               total_size += entry[n]->size;
               break;
           case FORM2:
               total_size += entry[n]->size + 2;
               if (!opts.single_track) total_size += PREGAP_SIZE;
               break;
           case DIR:
               total_size += 1;
               break;
           default:
               break;
       }
   }
   
   total_size += (opts.single_track ? 23 : 302);

   printf("Predicted CD size: %02d:%02d.%02d\n",
      LBA2MIN(total_size),
      LBA2SEC(total_size),
      LBA2FRA(total_size));
   }
*/

   /* Initialize ISO FS */

   for (i = 0; i < num_dirs; i++)
   {
       dir_entry[i] = (dir_s *) malloc (sizeof(dir_s));
       dir_entry[i]->name = (char *) malloc (MAX_PATH);
       dir_entry[i]->record = NULL;
   }

   /* PASS_1 just fills dir_entry[]->len values */

   mk_vcd_iso_fs(entry, num_entries, dir_entry, num_dirs, &opts, PASS_1);

   /* PASS_2 returns the first available extent after file system */

   START_FILE_EXTENT = mk_vcd_iso_fs(entry, num_entries, dir_entry, num_dirs, &opts, PASS_2);

   /* Convert extents to absolute */

   form1_extent += START_FILE_EXTENT;

   if (opts.single_track || form1_extent > ISO_FS_BLOCKS)
      ISO_FS_BLOCKS = form1_extent;

   ISO_FS_BLOCKS += 2; // add 2 empty blocks at the end of ISO track

   form2_extent += ISO_FS_BLOCKS;

   for(n=0;n<num_entries;n++)
   {
      if (entry[n]->type == FORM1)
         entry[n]->extent += START_FILE_EXTENT;

      if (entry[n]->type == FORM2)
         entry[n]->extent += ISO_FS_BLOCKS;
   }

   /* Show total CD size */

   last_extent = form2_extent;

   printf("Estimated CD size: %d MB (%02d:%02d.%02d). Actual data written: %d MB\n\n",
      (int)(last_extent*2048)>>20,
      LBA2MIN(last_extent),
      LBA2SEC(last_extent),
      LBA2FRA(last_extent),
      (int)((last_extent-ISO_FS_BLOCKS)*2324 + (ISO_FS_BLOCKS-START_FILE_EXTENT)*2048)>>20);

   /* Build the ISO track */

   printf("Building ISO filesystem...\n");

   /* PASS_3 outputs the actual file system */

   mk_vcd_iso_fs(entry, num_entries, dir_entry, num_dirs, &opts, PASS_3);

   /* Copy the files */

   printf("\nCopying files...\n");

// copy FORM1 files

   extent = START_FILE_EXTENT;

   for(n=0;n<num_entries;n++)
   {
      if (entry[n]->type != FORM1) continue; // only FORM1 files

      extent = copy_file(entry[n]);
      
      if (extent < 0)
      {
         fprintf(stderr,"Fatal Error --- exiting\n");
         fclose(xa_fd);
         remove(BINARY_OUTPUT_FILE);
         if (sub_fd) {
            fclose(sub_fd);
            remove(SUB_OUTPUT_FILE);
         }
         exit(1);
      }
   }

// fill gap between FORM1 & FORM2 files (if any)

   memset(data,0,2048);
   for(i=extent;i<ISO_FS_BLOCKS;i++)
      output_form1(i,data);

// copy FORM2 files

   extent = ISO_FS_BLOCKS;

   for(n=0;n<num_entries;n++)
   {
      if (entry[n]->type != FORM2) continue; // only FORM2 files

      /* Pregap */

      if (!opts.single_track)
      {
         tno = entry[n]->number + 1;
         track_start = entry[n]->extent;
         tindex = 0;

         memset(data,0,2324);
         for(i=0;i<PREGAP_SIZE;i++)
            output_form2(extent++,0,0,0x20,0,data);

         tindex = 1;
      }

      /* Copy file */

      extent = copy_file(entry[n]);

      if (extent < 0)
      {
         fprintf(stderr,"Fatal Error --- exiting\n");
         fclose(xa_fd);
         remove(BINARY_OUTPUT_FILE);
         if (sub_fd) {
            fclose(sub_fd);
            remove(SUB_OUTPUT_FILE);
         }
         exit(1);
      }

      /* Postgap (2 empty form 2 blocks at the end) */

      memset(data,0,2324);
      for(i=0;i<2;i++)
         output_form2(extent++,0,0,0x20,0,data);
   }

   /* Output cuesheet files */

   num_tracks = opts.single_track ? 1 : form2_file_number + 1;

   if (opts.imgformat != RAW)
   {
      fd_toc = fopen(TOC_FILE,"w");
      if(!fd_toc)
      {
         fprintf(stderr,"Can not open %s\n",TOC_FILE);
         perror("fopen");
         exit(1);
      }
      output_toc_file(fd_toc, entry, num_entries, num_tracks, last_extent, binfilename);
      fclose(fd_toc);

      fd_cue = fopen(CUE_FILE,"w");
      if(!fd_cue)
      {
         fprintf(stderr,"Can not open %s\n",CUE_FILE);
         perror("fopen");
         exit(1);
      }
      output_cue_file(fd_cue, entry, num_entries, num_tracks, binfilename);
      fclose(fd_cue);
   }

   if (opts.imgformat == CCD)
   {
      fd_ccd = fopen(CCD_FILE,"w");
      if(!fd_ccd)
      {
	      fprintf(stderr,"Cannot open %s\n",CCD_FILE);
	      perror("open");
	      exit(1);
      }
      output_ccd_file(fd_ccd, entry, num_entries, num_tracks, last_extent);
      fclose(fd_ccd);
   }
   
   /* Close files */

   BufWriteFlush(&bin_file_buffer);
   BufFree(&bin_file_buffer);
   fclose(xa_fd);

   if (opts.imgformat == CCD)
   {
      BufWriteFlush(&sub_file_buffer);
      BufFree(&sub_file_buffer);
      fclose(sub_fd);
   }

   printf("\nCD image creation was successful. Happy burning!\n");

   /* Free all buffers */

   for (i = 0; i < num_dirs; i++)
   {
       free(dir_entry[i]->record);
       free(dir_entry[i]->name);
       free(dir_entry[i]);
   }

   for (i = 0; i < num_entries; i++)
   {
	   free(entry[i]->name);
	   free(entry[i]);
   }

   exit(0);
}

