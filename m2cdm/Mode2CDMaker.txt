
Mode2 CD Maker v1.5.1
=====================

Disclaimer
----------

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


Purpose
-------

With this tool you can build a CD/XA Bridge image using any file you want
(not just MPEGs). What's the point? Well, using Mode2/Form2 you have bigger
user data sector sizes so you can fit more data in a single CD-R (up to 800
MB of data in a 80 min. disc).

What's the drawback, then? There is an issue on how Windows handles these
discs. It reads the files burned this way as RAW data, and it also appends
a RIFF/CDXA header at the beggining of the file. So you cannou burn anything
you want ant then just see it like any other file: you must deal with this
RIFF/CDXA stuff.

Here you have the theoretical max. capacities for each type of media:

          +--------+--------+--------+--------+
          | 74 min | 80 min | 90 min | 99 min |
 +--------+--------+--------+--------+--------+
 | Mode 1 | 650 MB | 700 MB | 790 MB | 870 MB |
 +--------+--------+--------+--------+--------+
 | Mode 2 | 738 MB | 795 MB | 896 MB | 987 MB |
 +--------+--------+--------+--------+--------+

The Mode 1 line shows the "original" CD capacity, the Mode 2 the one that
can be achieved using mode2cdmaker. In the practice you'll have to substract
approx. 1 MB from the Mode 2 values since it's reserved for the ISO Bridge
track. In my calculations 1 MB = 2^20 (1024*1024), and not this 1000*1000
stupid value used by the HD makers' marketing division.


Usage
-----

mode2cdmaker -m "movie1.avi" -d dirname -f "readme.txt" -v MODE2CD -e DAT

-h, -help    this help
-v, -volume  volume name (default: MODE2CD)
-o, -output  output image base name (*.bin, *.toc, *.cue) (default: image)
-f, -file    add Form1 files
-m, -movie   add movie (Form2) files (by default)
-d, -dir     add subdirectory
-e, -ext     set default extension for Form2 files (default: DAT)
-c, -dos     assume DOS charset for input file names
-x, -extname keep original Form2 file extension
-s, -single  create a single track image
-ccd         output CloneCD image (*.img, *.sub, *.ccd)
-raw         output RAW-96 image (*.img)
-list        use filelist for input Form1 files
-paramfile   use parameter file for input
-isolevel1   force ISO 9660 Level 1 for file names
-isolevel2   force ISO 9660 Level 2 for file names

* Add movie files: -m "d:\path\my movie.avi" [...]

This is the main option in Mode2 CD Maker. By the use of -m (or -movie) option 
you can add any number of files to the current CD structure that are entitled to 
be burned as Mode2/Form2. You can enter as many files as you want after a single 
-m option, provided you enter the full path for each one. You can also use 
wildcards such as "*.*" or "*.avi" instead of adding single files.

Note that by default the original file extension will be changed to ".DAT" for 
all Form2 files. This is needed because once burned as Form2, the original file 
contents won't be the same as the original, because of the way these files are 
readed by the OS (so you need the RIFF/CDXA reader filter to be able to play 
them). You can change this behaviour with the options below.

* Add other files: -f "d:\path\my file.ext" [...]

The -f (or -file) option works exactly equal as the -m option but in this time 
the files specified here will be recorded as Form1, not taking any advantage of 
the extra CD space, but you can read them just like any other file on the HD. 
This is meant for executable/text and other files that are not supported through 
DirectShow (so you can't read them using the RIFF/CDXA filter). As an example, 
you can add drivers, players or text files to the CD structure this way.

Unlike Form2 files, these will retain their original file name and extension, 
since no change is made to their contents.

* Add directory: -d "dir name"

By the use of the -d (or -dir) option you can add a new subdirectory to the 
current CD structure. After this option any file entered using the -m or -f 
option will lie on the last directory specified. Note that you can't add full 
paths from the HD inluding its directory structure using the -m or -f option, 
hence the usefulness of this option.

An example: -m "movie.avi" -d "player" -f "c:\my favorite player\*.*"

This will add movie1.dat to the root directory, make a new dir named "player" 
and add the contents of the path specified after -f option to the "player" 
directory on the CD (not including subdirectories, though).

Each directory entered after the -d option will be created in root. If you want 
to specify a subdirectory tree, separate each directory level with backslashes, 
but remember to create their parent directory first. An example of nested 
subdirectories:

-d "player" -f "c:\my player\*.*" -d "player\filters" -f "c:\my player\filters\*.*"

* Force ISO 9660 Level 1 or 2 file names: -isolevel1, -isolevel2

By default all names are saved in ASCII form (i.e. they maintain their original 
uppercase/lowercase characters and their length). In case you want to fit to 
standards you can force ISO 9660 Level 1 (i.e. all uppercase, 8+3 filenames) or 
Level 2 (i.e. all uppercase, max. 31 chars per file name) conversion to all file 
names. As an example a file called "My Movie.avi" would become "MY_MOVIE.DAT" 
after being converted to ISO Level 1.

* Change default Form2 extension: -e "ext"

By default all Form2 files will change their extension to "DAT" as stated above. 
You can change this default extension by using your favorite one thanks to the -
e (or -ext) option.

However note that by any means it's recommended using a registered extension 
here since it may lead to problems when trying to play the file (this is the 
case with OGG/OGM, as an example).

* Keep original Form2 extension into file name: -x

You can keep the original Form2 file extension into the file name by adding the 
-x option to the command line.

An example: -m "this is a sample movie.avi" -x

Will create "this is a sample movie.avi.dat"

* Use DOS charset for source file names: -c

In case you want to enter file names with international character sets using DOS 
character set (this is useful for BAT files, and some languages such as Russian) 
you can add the -c (or -dos) option to tell the program to do the appropiate 
conversion before trying to open the file. Otherwise you'd get a "file not 
found" error.

* Set CD volume name: -v "name"

With this option you can specify a new volume name for the current CD, other 
than the default "MODE2CD".

* Set output image base name: -o "d:\path\image"

With the -o (or -output) option you can specify a default path and base file 
name for the output image. The default path is current directory and the base 
name "image". Three files will be created using this information: *.cue, *.bin 
and *.toc.

An example: -o "d:\cdimages\image"

This will create: image.bin, image.cue and image.toc under "d:\cdimages" path.

* Create a single track image: -s

One of the limitations of standard Mode2/Form2 CDs is the minimum track size 
(300 sectors), so any file being recorded as Mode2/Form2 must be greater than 
300 Kb in size. On the other side every track must be preceeded by a pregap 
which adds extra 300 Kb for each Form2 file, making it unsuitable for a lot of 
small files such as MP3.

In that case you can add the -s (or -single) option which forces the creation of 
a single-track CD. This makes all files lie on a single track, which has the 
following advantages: now each Form2 file can be smaller than 300 Kb, no pregaps 
are added for each one (thus saving more space) and the ISO filesystem is 
smaller, too, because there is no need for a separate ISO Bridge track.

This makes a better use of the available CD space, specially for large file 
collections where previously the tool would have not been able to take advantage 
of the extra space (since all gained space was then lost on pregaps).

As a disadvantage, these CDs are currently unusable from Linux since the 
software currently available under Linux that can read XCDs works in a track-
basis and not in a filesystem-basis, so it needs each Form2 file to lie on a 
separate track. Examples of this are mplayer as well as the vcdfs and cdfs 
drivers. Unfortunately, seems Linux can't read XCDs the same way as Windows due 
to a lack of support for Form2 files in the iso9660 driver (they are readed as 
Form1 files).

* Output CloneCD (CCD) image: -ccd

New to 1.5.1 is the possibility to create a CloneCD (CC/IMG/SUB) image set, instead
of the default BIN/CUE image. These images contain more accurate information about
every sector and thus gives less compatibility problems with certain recorders. The
only drawback is that you need a compatible recording tool (such as CloneCD or
Alcohol 120%) in order to burn them.

* Output RAW-96 image: -raw

With this option you can create a single file RAW-96 image, which is intended to be
recorded with with recent cdrecord versions using the -clone mode. Only single track
images can be created this way. This feature has not been tested, though.

* Input Form1 file list: -list listfile.txt

With this option you can enter a simple filelist contained in a text file to be
recorded as Form1 content. Every line may contain only a single file with its
complete path. This way you can overcome the max command line length limit when
entering a big amount of files. Subdirectories cannot be created this way (use it
as an equivalent to the -f option). Paths containing spaces should NOT be enclosed
between quotes, as this is a command-line only requirement.

* Input parameter file: -paramfile params.txt

This option replaces the entire command line structure and takes every option from
a parameter file. The purpose of this is overcoming the max command line lenght.
Every single parameter must stay in a single line, ending with a line feed.
Otherwise the usage is the same as the command line. As with the previous option,
you should NOT enclose paths containing spaces between quotes. An example of a
parameter file:

-m
d:\movie\my favorite movie.avi
-d
player
-f
d:\my player\*.zip
-o
d:\burn\cdimage
-ccd
-s
-v
Movie Title


Burning
-------

Beware that if the source Form2 files are not an exact multiple of 2324
bytes, padding zeroes will be added at the end. There's nothing I can do
about this, this is a limitation of Mode2/Form2.

While building the image the program will show an estimated CD size required
to hold the data you are putting into the image. This size value is
calculated in terms of Mode 1, so you can easily compare it with the actual
CD size as shown when you retrieve the media information with your favorite
burning program (i.e. 700 MB means a full 80 min CD).

Once it's finished you will end with three files:

image.cue      CUE file for CDR-WIN
image.toc      TOC file for cdrdao
image.bin      BIN image

To burn with CDR-WIN just load the *.cue file and burn it.

To burn with cdrdao:

> cdrdao scanbus
> cdrdao write --device 1,0,0 --driver generic-mmc image.toc

In case you chose CloneCD output five files are created by default:

image.ccd      CloneCD cuesheet
image.sub      Subchannel data
image.img      Binary image
image.cue      CDR-WIN CUE for compatibility
image.toc      TOC file for cdrdao

Just load the image.ccd file into CloneCD or Alcohol 120% and burn it.

If you have troubles you can also use any other recording software that
supports raw BIN images, however this won't be a real CD/XA Bridge, but a
single track, mixed Form1/Form2 CD. anyways Windows can't see the
difference, and it works fine. To burn with Nero as a single track:

1. Choose File/Burn Image menu
2. Load the BIN file
3. Select Mode2
4. Burn!


Reading
-------

To be able to play these files directly from the CD, you'll need either a 
DirectShow filter supporting RIFF/CDXA parsing (such as XCD reader filter made 
by avih & me), or a player with this capability. As an example there is at least 
a portable MP3 player which has been proved to directly support Mode2 Form2 CDs: 
the Freecom Beatman CD/MP3 Player.

You can also convert the files back to its original state with a helper tool
I've included called dat2file. The usage is:

> dat2file "d:\my movie.dat" "c:\my movie.avi"

Beware that if the source file was not an exact multiple of 2324, there will
be some padding zeroes at the end of the file. This might prevent it of
being played properly.


Credits
-------

mode2cdmaker is based on vcdtools-0.4 by Rainer Johanni

dat2file is based on cdxa2mpeg by Herbert Valerio Riedel

Many thanks to Nic, int 21h, Nah, ubik29 & Martin for their contributions to
this tool, and of course MaTTeR and Kxy for their extensive testing!


Official homepage: http://webs.ono.com/de_xt/

DeXT @ 2003


History
-------

2003/03/14 - 1.5.1 - Alex Noe & DeXT
    
mkvcdfs.c:  added CloneCD (CCD/SUB/IMG) image output support (Alex Noe)
mkvcdfs.c:  added filelist input support, as well as parameter file (Alex Noe)
mkvcdfs.c:  cuesheets are only written after the image is created
mkvcdfs.c:  enhanced file buffering

2002/11/12 - 1.5a - DeXT

mkvcdfs.c:  fixed *stupid* bug that caused Mode2CDMaker to crash when no
            subdirectories were entered (sorry)

2002/11/07 - 1.5 - DeXT

mkvcdfs.c:  added -s option (creates single track image, removes minimum file
            size limit, removes pregaps, better use of available space,
            useful for many small Form2 files such as MP3 collections)
mkvcdfs.c:  new command line options parser, options are more user friendly
mkvcdfs.c:  by default all file names are now kept as ASCII (-l and -a
            options removed), added -isolevel1 and -isolevel2 options
mkvcdfs.c:  better memory management: buffers are now allocated at run-time
mkvcdfs.c:  added 2 empty blocks at the end of ISO track to avoid potential
            reading problems when no Form2 files where added
mkvcdfs.c:  show actual data written to the CD along with estimated CD size
vcdisofs.c: each directory can now contain an unlimited number of files (it
            was previously limited to about 32 per directory)
vcdisofs.c: added nested subdirectories support (now you can maintain the
            full directory structure)
vcdisofs.c: better use of ISO track space (files are being recorded right
            after the ISO FS)
vcdisofs.c: fixed international character set support for subdirs, too

2002/08/26 - 1.4.2 - ML & DeXT

mkvcdfs.c:  added ML's wildcard code (for MSVC)
mkvcdfs.c:  added -c option (assume DOS character set for input file names,
            useful from BAT and also some languages, such as Russian)
vcdisofs.c: properly support international file names on the CD

2002/06/24 - 1.4.1

mkvcdfs.c:  fixed incorrect track numbering in CUE file

2002/06/17 - 1.4

mkvcdfs.c:  big speedup thanks to new r/w buffering (up to 3 times faster)
mkvcdfs.c:  BIN image is now referenced as relative in TOC, CUE files
mkvcdfs.c:  new option: -x (keep form2 file extension in the filename)
mkvcdfs.c:  new file collect routine, to allow subdirs for form2 files
mkvcdfs.c:  default screen output is now stdout (so it can be redirected)
mkvcdfs.c:  fixed bug where input files were not closed
vcdisofs.c: subdirs now work for form2 files, too
vcdisofs.c: fixed max filename length calculation (was greater than allowed)
vcdisofs.c: ISO building is now faster for form1 files (no need to reserve space)

2002/05/23 - 1.3

mkvcdfs.c:  new options added: -d, -a
vcdisofs.c: added subdirectories support (for Form1 files only)
vcdisofs.c: added ASCII character set support
vcdisofs.c: removed default VCD-like filenames for Form2 files

2002/05/20 - 1.2

mkvcdfs.c:  command line parser is now less tolerant to bad syntax
mkvcdfs.c:  ISO FS is now built before copying actual file data
mkvcdfs.c:  show total estimated CD size
mkvcdfs.c:  new XA subheader which is more compatible with DVD players
vcdisofs.c: added Form1 files support (through new option -f)

2002/05/10 - 1.2pre2

mkvcdfs.c:  added progress counter and speed indicator
mkvcdfs.c:  fixed DAT filesize (was 1 sector higher w/multiple of 2324)
mkvcdfs.c:  fixed CUE file not being deleted on failure
mkvcdfs.c:  XA subheader now marks EOF
vcdisofs.c: improved XA compatibility (added XA sig for every file/dir)
vcdisofs.c: added long file names support (ISO-9660 Level 2)
vcdisofs.c: fixed emtpy block in ISO track
dat2file:   added nice counter, too
dat2file:   no need to enter destination file name

2002/05/04 - 1.2pre

mkvcdfs.c:  new command line option parser (Nic)
mkvcdfs.c:  new features (set volume name, M2F2 ext, output image) (Nic)
mkvcdfs.c:  changed default volume name and image base name
mkvcdfs.c:  added simple d-char (ISO-9660) converter
vcdisofs.c: M2F2 file extension is no longer fixed

2002/04/18 - beta 1.1

mkvcdfs.c:  added CUE output support
vcdisofs.c: lowered ISO track size to 302 sectors (~700 Kb)

2002/04/17 - beta 1.0 - first release, based on vcdtools-0.4
 
mkvcdfs.c:  changed output file method to fopen() (open() worked in text
            mode, perhaps a MinGW bug?)
mkvcdfs.c:  removed 30+45 empty sector padding in M2F2 track
mkvcdfs.c:  now will accept any file (not just MPEG)
mkvcdfs.c:  changed CD/XA track subheader to Form2/Data
mkvcdfs.c:  fixed DAT file length and track file length:
            * DAT file now has right size (missed 1 sector)
            * CD/XA track now has 2 "ghost" sectors at the end, to
              prevent any potential TAO problems
vcdisofs.c: fixed make_path_tables() to not add root files as dirs
vcdisofs.c: DAT files are added to the root directory instead of MPEGAV
vcdisofs.c: removed unneeded VCD stuff
