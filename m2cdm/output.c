#ifdef _WIN32
#include <windows.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include "defaults.h"
#include "raw96.h"

void output_ccd_entry(FILE *fd_ccd, int entry, int point, int a_lba, int p_lba)
{
	   fprintf(fd_ccd,"[Entry %d]\n", entry);
	   fprintf(fd_ccd,"Session=1\n");
	   fprintf(fd_ccd,"Point=0x%02x\n", point); // BCD
	   fprintf(fd_ccd,"ADR=0x01\n");
	   fprintf(fd_ccd,"Control=0x04\n");
	   fprintf(fd_ccd,"TrackNo=0\n");
	   fprintf(fd_ccd,"Amin=%d\n", LBA2MIN(a_lba));
	   fprintf(fd_ccd,"ASec=%d\n", LBA2SEC(a_lba));
	   fprintf(fd_ccd,"AFrame=%d\n", LBA2FRA(a_lba));
	   fprintf(fd_ccd,"ALBA=%-d\n", a_lba);
	   fprintf(fd_ccd,"Zero=0\n");
	   fprintf(fd_ccd,"PMin=%d\n", LBA2MIN(p_lba));
	   fprintf(fd_ccd,"PSec=%d\n", LBA2SEC(p_lba));
	   fprintf(fd_ccd,"PFrame=%d\n", LBA2FRA(p_lba));
	   fprintf(fd_ccd,"PLBA=%-d\n", p_lba);
}

void output_ccd_file(FILE *fd_ccd, entry_s **entry, int num_entries, int num_tracks, int last_extent)
{
int ccd_entry = 0;
int n;

	   fprintf(fd_ccd,"[CloneCD]\n");
	   fprintf(fd_ccd,"version=3\n");

	   fprintf(fd_ccd,"[disc]\n");
	   fprintf(fd_ccd,"TocEntries=%d\n", num_tracks + 3);
	   fprintf(fd_ccd,"Sessions=1\n");
	   fprintf(fd_ccd,"DataTracksScrambled=0\n");
	   fprintf(fd_ccd,"CDTextLength=0\n");

	   fprintf(fd_ccd,"[Session 1]\n");
	   fprintf(fd_ccd,"PreGapMode=2\n");
	   fprintf(fd_ccd,"PreGapSubC=1\n");

	   ccd_entry = 0;
	   output_ccd_entry(fd_ccd, ccd_entry++, 0xa0, MSF2LBA(0,0,0), MSF2LBA(1,0x20,0));
	   output_ccd_entry(fd_ccd, ccd_entry++, 0xa1, MSF2LBA(0,0,0), MSF2LBA(num_tracks,0,0));
	   output_ccd_entry(fd_ccd, ccd_entry++, 0xa2, MSF2LBA(0,0,0), last_extent);
	   output_ccd_entry(fd_ccd, ccd_entry++, 0x01, MSF2LBA(0,0,0), MSF2LBA(0,2,0));

       if (num_tracks > 1) {
          for (n=0;n<num_entries;n++) {
              if (entry[n]->type != FORM2) continue;
              output_ccd_entry(fd_ccd,
                               ccd_entry++,
                               bin2bcd(entry[n]->number + 1),
                               MSF2LBA(0,0,0),
                               entry[n]->extent);
          }
       }

	   fprintf(fd_ccd,"[TRACK 1]\n");
	   fprintf(fd_ccd,"MODE=2\n");
	   fprintf(fd_ccd,"INDEX 1=0\n");

       if (num_tracks > 1) {
          for (n=0;n<num_entries;n++) {
              if (entry[n]->type != FORM2) continue;
                 fprintf(fd_ccd,"[TRACK %d]\n", entry[n]->number + 1);
                 fprintf(fd_ccd,"MODE=2\n");
                 fprintf(fd_ccd,"INDEX 1=%d\n", entry[n]->extent);
          }
       }
}

void output_toc_file(FILE *fd_toc, entry_s **entry, int num_entries, int num_tracks, int last_extent, char *binfilename)
{
int track_size;
int n;

      fprintf(fd_toc,"CD_ROM_XA\n\n");
      fprintf(fd_toc,"// Track 1: Header with ISO 9660 file system\n");
      fprintf(fd_toc,"TRACK MODE2_RAW\n");
      if (num_tracks > 1)
         track_size = ISO_FS_BLOCKS; // ISO track only
      else
         track_size = last_extent; // total CD size
         
      if (num_tracks > 1)
         track_size += PREGAP_SIZE; // add next track pregap

      fprintf(fd_toc,"DATAFILE \"%s\" %02d:%02d:%02d\n\n",
         binfilename,
         BLOCKS2MIN(track_size),
         BLOCKS2SEC(track_size),
         BLOCKS2FRA(track_size));

      if (num_tracks > 1) {
          for (n=0;n<num_entries;n++) {
              if (entry[n]->type != FORM2) continue;
              fprintf(fd_toc,"// Track %d: data from %s\n", entry[n]->number + 1, entry[n]->name);
              fprintf(fd_toc,"TRACK MODE2_RAW\n");
              track_size = entry[n]->size + 2; // track size
              if ((entry[n]->number + 1) != num_tracks)
                 track_size += PREGAP_SIZE; // all but last track (pregaps)
              fprintf(fd_toc,"DATAFILE \"%s\" #%d %02d:%02d:%02d\n\n",
                 binfilename,
                 entry[n]->extent*2352,
                 BLOCKS2MIN(track_size),
                 BLOCKS2SEC(track_size),
                 BLOCKS2FRA(track_size));
         }
      }
}

void output_cue_file(FILE *fd_cue, entry_s **entry, int num_entries, int num_tracks, char *binfilename)
{
int n;

      fprintf(fd_cue,"FILE \"%s\" BINARY\n", binfilename);
      fprintf(fd_cue,"  TRACK 01 MODE2/2352\n");
      fprintf(fd_cue,"    INDEX 01 00:00:00\n");

      if (num_tracks > 1) {
          for (n=0;n<num_entries;n++) {
              if (entry[n]->type != FORM2) continue;
              fprintf(fd_cue,"  TRACK %02d MODE2/2352\n", entry[n]->number + 1);
              fprintf(fd_cue,"    INDEX 01 %02d:%02d:%02d\n",
                 BLOCKS2MIN(entry[n]->extent),
                 BLOCKS2SEC(entry[n]->extent),
                 BLOCKS2FRA(entry[n]->extent));
          }
      }
}

