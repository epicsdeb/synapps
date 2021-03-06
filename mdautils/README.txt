MDA Utilities v1.3.1
February 2014

Written by Dohn A. Arms, Argonne National Laboratory
Send comments to dohnarms@anl.gov

See LICENSE file for licensing of these programs and code.

The location of these utilities on the Internet is:
http://www.aps.anl.gov/bcda/mdautils/

--------------------------------------------------------------


These utilities are used for accessing MDA files generated by the
saveData routine in EPICS, and can handles MDA files of any dimension.

The command line utilities all give help with the "-h" option, and
show version information with the "-v" option.  Documentation is also
included in the "doc" subdirectory.  Each utility has a man page (with
a PDF version included), while the library comes with a manual in PDF
form.


Utilities:
-----------

1) mda2ascii - This program converts MDA files to ASCII files, with
various options, so the data can be read directly into a
plotting/fitting program.

2) mda-info - This program will print to the screen information about
an MDA file.  It shows things such as the time, the dimensionality,
positioners, detectors, and triggers used.  This program does not load
the entire file, just the relevant information, making it fast.

3) mda-ls - This program print to the screen a listing of all the MDA
files in the current or a specified directory.  It shows the name,
dimensionality, positioners, and optionally the time for each file.
It is possible to filter the listing to those files including a
certain positioner, detector, or trigger.

4) mda-dump - This program will dump to the screen the entire contents
of an MDA file, exactly as how they appear in the file.  This program
can be useful for debugging purposes, as it prints out information
immediately after loading it.  It does not use the mda-load library,
instead using direct functions for reading the file.

5) mdatree2ascii - This script converts a directory tree of MDA files
into a new parallel directory tree populated by ASCII files, ignoring
all non-MDA files.  It uses the program mda2ascii to do the actual
conversion.

6) mda-load library - This is the engine used for reading the MDA
files.  The library functions are relatively simple, as there are very
few.  Accessing the data -- due to the arbitrary dimensional nature of
the MDA files -- can be complicated, and familiarity with structures
and pointers in C is a must.  The manual for it is mda-load.pdf, and
reading mda-load.h can be helpful.  It could be built as a shared
library, but since it is rather small and different systems utilize
shared libraries differently, this isn't enabled.



Requirements:
-------------

MDA Utilities can be compiled using a C99-compatible compiler (such as
gcc), make, and ar (if the library is to be made).  C99 compatiblity
is needed only so far as <stdint.h>.  MDA Utilities have been
successfully compiled on Linux, Solaris, and Mac OS X, while it can
also be compiled on Windows (I use MinGW).

The only extra library requirement is access to the standard XDR
routines.  With Linux and Mac OS X, they're part of the standard C
library; with Solaris, they're part of the standard Networking
Services Library (nsl). No extra packages should have to be installed
with these systems.

Windows does not come standard with XDR routines.  Either an extra
library has be used, or an included XDR reading hack can be enabled
(using the xdr_hack code). Either way, the Makefile has to be modified
to make this all work.

The program mdatree2ascii is a script, and needs the following
programs (other than mda2ascii): bash, find, and sed.  These programs
are very standard (other than on Windows), and should already be
installed on your system.



Compiling:
----------

If building from source code, except for Windows, all you need to do
is type "make" when in the source directory, and the executables and
library should be made.

With Windows, the Makefile has to be edited.  The line in the Windows
comment block has to be uncommented.  If using the included XDR
support, the first two lines of the XDR block as well as the
little-endian (LE) line have to be uncommented.  The names of the
resulting executables need to be renamed to inlude the .exe suffix as
well.  The GCC and AR definitions might have to be changed.



Installing:
-----------

To install the utilities in "/usr/local", simply run "make install".
If you want the files in a different directory, change the "prefix"
variable in the file Makefile, then run "make install".  Otherwise,
you can copy the files wherever you like.

