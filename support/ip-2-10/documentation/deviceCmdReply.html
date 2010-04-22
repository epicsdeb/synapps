<!doctype html public "-//w3c//dtd html 4.0 transitional//en">
<html>
<head>
   <meta http-equiv="Content-Type" content="text/html; charset=iso-8859-1">
</head>
<body text="#000000" bgcolor="#FFFFFF" link="#0000EE" vlink="#551A8B" alink="#FF0000">

<style type="text/css">
em {font-style: normal; color: red }
code {font-style: normal; color: blue }
pre {font-style: normal; color: blue }
blockquote {font-size: 85%}
</style>

<center><h1>deviceCmdReply</h1></center>

<h2>Introduction</h2>

deviceCmdReply is an EPICS database that can be programmed at run time to
<i>integrate into EPICS</i> a <i>message based</i> device for which no EPICS
support has been written.  A single deviceCmdReply database can format and send
one command string to a device and then read and parse one reply string. 
Strings are limited to 39 bytes, and may contain any ASCII characters,
including the null character, They also may contain any checksum or CRC
supported by the sCalcout record.

<P><blockquote><i>Integrate into EPICS</i> means "provide an EPICS PV that
represents a value in the device", so that if one writes to the EPICS PV, the
value gets written to the device; or if one reads from the EPICS PV, one gets a
value from the device.   (Note that writing to an EPICS PV via channel access
can cause the processing that gets the value out to the device, but reading
from a PV via channel access will not cause processing to occur. If you want a
PV to track some value in  the device, you must arrange for that value to be
read.  For example, you might configure the deviceCmdReply database to process
periodically.) </blockquote>

<P><blockquote> A <i>message based</i> device is one that communicates with its
user via sequences of bytes -- typically, ASCII character strings.  Message
based devices typically communicate via serial, GPIB, or socket (TCP/IP or
UDP/IP) interfaces.</blockquote>

<P>deviceCmdReply is essentially a wrapper around the EPICS <a
href="http://www.aps.anl.gov/epics/modules/soft/asyn/R4-6/asynRecord.pdf">asyn
record<a>.   The deviceCmdReply database consists essentially of two sCalcout
(string-calc-and-output) records -- one to format the command, one to parse the
reply -- and an asyn record, which performs the actual writing and reading. 
The asyn record provides most of the raw capabilities of deviceCmdReply.  Among them
are the following:

<P><table border>

<tr>
<td>write to and read from serial, GPIB, or socket interface
<td>This allows deviceCmdReply to control a wide range of devices.

<tr><td>connect several asyn records to a single port

<td>This allows multiple instances of deviceCmdReply to work together to control
different aspects of a single device.  Infrastructure supporting the asyn record
keeps multiple intances of deviceCmdReply from interfering with each other, and
with other asyn-based support talking to the same device.

<P>This capability also permits deviceCmdReply to <i>supplement</i> existing
support for a message based device.

<tr>
<td>disconnect from one port and connect to another port without interfering
with ongoing port traffic

<td>This permits one to load a small number of deviceCmdReply databases, whose
eventual use may not even be known at load time, and to target as many of those
databases as are needed at a particular device, to support the required set of
commands.

<tr>
<td>modify port configuration at run time
<td>This allows the user to try, for example, different baud rates and
handshaking arrangements, to find one that works.

<tr>
<td>show commands and replies as they actually are sent and received
<td>This allows the user quickly and efficiently to debug command formatting,
reply parsing, and interface configuration.

<tr><td>
<td>

<tr><td><td>

</table>



<P>Thus, several instances of deviceCmdReply can be targeted at a single device,
to implement different commands, or read different values.  For example, a
single deviceCmdReply might periodically read the readback temperature from a
controller, while another deviceCmdReply is used to write the temperature set
point.

<h3>Other ways to write device support</h3>

deviceCmdReply is a quick way to get something running in a pinch, and it's a
nice tool for prototyping -- for trying out commands and port configurations,
to see how a device behaves.  But it's not the best way to write real device
support.  Better strategies for writing message based device support include
the following:

<P><table border>

<tr><td>streamDevice<td>Connects standard EPICS record types directly to
hardware, using a protocol file to specify the command formatting, reply
parsing, etc.  See <a
href="http://epics.web.psi.ch/software/streamdevice/">streamDevice 2</a>

<tr><td>devXxStrParm<td>Connects selected record types directly to hardware.
Command formatting and reply parsing are specified with in the user-parameter
section of output or input links. See devXxStrParm.README in the documentation
directory of the synApps <b>ip</b> module.

<tr><td>SNL<td>Typically, SNL code monitors PV's and writes to/reads from hardware using
an asyn record.

<tr><td>Other<td>There are other approaches in use, but I don't know enough
about them to describe how they work.

</table>

<h2>How to deploy deviceCmdReply</h2>

<P>deviceCmdReply is part of the synApps <b><a
href="http://www.aps.anl.gov/aod/bcda/synApps/ip/ip.html">ip</a></b> module, and
it requires the EPICS <b><a
href="http://www.aps.anl.gov/epics/modules/soft/asyn">asyn<a></b> module and the
synApps <b><a
href="http://www.aps.anl.gov/aod/bcda/synApps/calc/calc.html">calc</a></b>
module to operate. This documentation assumes version 2.7 of the <b>ip</b>
module, version 4.6 or higher of <b>asyn</b>, and version 2.6.3 or higher of
<b>calc</b>.

<h3>Building and installation</h3>

The recommended way to build this software is to build synApps, which includes
it, supports it, and provides an example ioc directory that deploys and uses it.
It certainly is both possible and practical to build and install only the
modules required for this particular piece of software, but we don't have the
staff to write documentation that describes the building and installation of
individual pieces of synApps.  (The number of possible combinations of required
modules is huge -- undoubtedly far larger than the number of custom
installations that might actually occur -- and it changes as synApps develops. 
So if we did write such arrangement-specific documentation, <i>most</i> of it
would never even be read.)

<h3>Loading into an ioc</h3>

<P>The database is loaded into an ioc with the following example command:

<P><pre>dbLoadRecords("$(IP)/ipApp/Db/deviceCmdReply.db",
    "P=xxx:,N=1,PORT=serial1,ADDR=0,OMAX=40,IMAX=40")</pre>

<P>where <code>$(IP)</code> will be expanded to the value of the environment
variable <code>IP</code> -- the full path to the <b>ip</b> module.  (The EPICS
build will put this in the <code>cdCommands</code> file if <code>IP</code> is
defined in the file <code>configure/RELEASE</code>.)

<P>The following macro arguments target or configure the database to a
specific application:

<dl>

<dt><code>P=xxx:</code><dd> defines a short sequence of characters intended to
distinguish record names in this ioc from the names of similar records
loaded into some other ioc

<dt><code>N=1</code><dd>defines another short sequence of characters intended to
distinguish the different deviceCmdReply databases loaded into the same ioc from
each other

<dt><code>PORT=serial1</code><dd> defines the <i>port</i> to which the asyn
record will connect initially.  (The port can be changed at run time.)

<dt><code>ADDR=0</code><dd>ignored unless the port to which the asyn record
connects can communicate with more than one device.  For example, if the port is
a GPIB interface, or an RS485 serial interface, <code>ADDR</code> specifies
which of several devices is to be written to or read from.  (The address can be
changed at run time.)

<dt><code>OMAX=40</code><dd>tells the asyn record how much space to allocate for
its binary output array <code>BOUT</code>.  This matters only if
<code>BOUT</code> is used, which happens only if the asyn record's
<code>OFMT</code> field is set to "Binary" or "Hybrid".  If <code>OFMT</code> is
set to "ASCII" (the default), then the <code>AOUT</code> field is used instead
of <code>BOUT</code>.  <code>AOUT</code> is an EPICS string, with a fixed size
of 40 bytes.

<dt><code>IMAX=40</code><dd>tells the asyn record how much space to allocate for
its binary input array <code>BINP</code>.  This matters only if
<code>BINP</code> is used, which happens only if the asyn record's
<code>IFMT</code> field is set to "Binary" or "Hybrid".  If <code>IFMT</code> is
set to "ASCII" (the default), then the <code>AINP</code> field is used instead
of <code>BINP</code>. <code>AINP</code> is an EPICS string, with a fixed size of
40 bytes.

</dl>

<P>This database will contain the following records by which the user programs
the device:

<P><table border>
<tr><th>record name<th>record type<th>function
<tr><td><em>xxx:</em>deviceCmdReply<em>n</em>_formatCmd<td>sCalcout<td>build the string to be written to the device
<tr><td><em>xxx:</em>deviceCmdReply<em>n</em>_do_IO<td>asyn<td>send to/receive from hardware
<tr><td><em>xxx:</em>deviceCmdReply<em>n</em>_parseReply<td>sCalcout<td>parse reply string
</table>

<P>where <em>xxx:</em> was specified by the <em>P</em> macro, and
<em>n</em> was specified by the <em>N</em> macro.


<P>From now on, I'll call these records "..formatCmd", "the asyn record",
and "...parseReply", respectively.

<h3>Arranging for save/restore </h3>

It would be very inconvenient to have to reprogram a set of deviceCmdReply
databases after every ioc reboot, and because they are designed to be
programmed by users on a running ioc, one would like to have the databases
autosaved.  You can do this by adding the following line to an autosave request
file:

<P><pre>file deviceCmdReply.req P=$(P),N=1</pre>

<P>where <code>P</code> and <code>N</code> are the same as in the above
<code>dbLoadRecords</code> command, for each database loaded.

<h3>Loading the user interface</h3>

Several MEDM display files are provided in the modules that together support
deviceCmdReply.  The <b>ip</b> module contains the display pictured below, and
abbreviated versions of it, which are the main user interface for a
single instance of deviceCmdReply.  These displays contain buttons that call up
related displays maintained by the <b>calc</b> and <b>asyn</b> modules, which
provide detailed control and some user documentation for the sCalcout and asyn
records used in deviceCmdReply.

<P><center><img src="deviceCmdReply_full.adl.jpg"></center>

<P>This display can be called up from another MEDM display with a "Related
Display" button defined with the same macro arguments as in the dbLoadRecords()
command above:

<P><center><table border>
<tr><th>Display Label<th>Display File<th>Arguments
<tr><td>whatever<td>deviceCmdReply_full.adl<td>P=xxx:,N=1
</table></center>


<h2>Programming</h2>

<ol>

<P><li> The first choice to make in the programming of a deviceCmdReply instance
is the port name that will get us connected to the device we want to control. 
Near the center of the display, next to the words "Send/Receive", is a
text-entry field containing the name of the port to which the asyn record is
connected.  This field belongs to the asyn record; its full name is
"<em>xxx:</em>deviceCmdReply<em>n</em>_do_IO.PORT".  You can change this to any
port that supplies an asynOctet interface.

<P><li>The next choice is what input/output operations are to be performed.  The
asyn record's <code>TMOD</code> field (labelled "Transfer:" in this display, and
in the asyn record's "asynOctet.adl" display) controls this, and provides the
following options:

<P><table border>
<tr><td>Write/Read<td>Send a command and wait for a reply
<tr><td>Write<td>Send a command
<tr><td>Read<td>Wait for a reply
<tr><td>Flush<td>Not used in deviceCmdReply
<tr><td>NoI/O<td>Useful for testing, and for disabling output while the
"..formatCmd" record is being configured. 
</table>

<P>When <code>TMOD</code> includes "Write", the first sCalcout record
("...formatCmd") is used to format the string to be sent to the device.  It
places the formatted string into the asyn record's <code>AOUT</code> field, and
causes the asyn record to process.

<P>When <code>TMOD</code> includes "Read", the second sCalcout record
("...parseReply") is processed by the asyn record after the device read has
completed.  It retrieves the string read by the asyn record from the asyn
record's <code>AINP</code> or <code>BINP</code> field, depending on the asyn
record's <code>IFMT</code> field.  If <code>IFMT</code> = ASCII, then 
<code>AINP</code> is used, otherwise <code>BINP</code> is used.

<P><li>If <code>TMOD</code> includes "Write", the next job is to program the
"...formatCmd" sCalcout record to craft an output command string from the
information available to it.  The information available to an sCalcout record
includes any numeric or string value that has been written to one of its fields,
and the values of any other EPICS PVs to which the sCalcout record can connect
an input link.  Note that the asyn record will append a terminator to the
command if the asyn record's <code>OEOS</code> field (the text field labelled
"TERM" in the "Output" section of the display above) is not empty.  The
terminator can be any one or two character string.  Common terminators include
"\r" (carriage return) and "\r\n" (carriage return, line feed).

<P><li>If <code>TMOD</code> includes "Write", the next job is to configure the
asyn record to talk to the port that connects with the device.  Click on
the "I/O details" related display button to see a menu of the asyn-record
displays with which this can be done.  For example, you might select
"Serial port parameters" to specify the baud rate, etc.

<P><li>If <code>TMOD</code> includes "Read", the next job is to configure the
asyn record so that it can recognize when the device has finished sending a
reply.  Three strategies for recognizing the end of a transmission are supported
by the asyn record:
<ol>
<li>Some devices append a terminator by which the end of string can be
recognized.  In this case, set the asyn record's <code>IEOS</code> field (the
text field labelled "TERM" in the "Input" section of the display above) to
the terminator the device will use.  (This terminator will be stripped from
the string before it is displayed and passed to the "...parseReply" sCalcout
record.)
<li>Other devices will send a predictable number of characters.  In this case,
set the asyn record's <code>NRRD</code> field (the text field labelled "Length
Requested", in the "Input" section of the above display) to the expected number
of characters
<li>If the device will do neither of the above, set the asyn record's <code>TMOT</code>
field (the text field labelled "Timeout:") to the number of seconds after
which the reply is certain to have arrived.
</ol>

<P><li>If <code>TMOD</code> includes "Read", the next job is to program the
"...parseCmd" sCalcout record to parse the string returned by the device,
and extract from it the number or string in which you're interested.
</ol>

<h2>Examples</h2>

<h3>Formatting printable output strings</h3>

For most devices, command strings are pretty simple: some fixed text, followed
by a number encoded in some way, maybe followed by some more fixed text.  Simple
strings with embedded numbers are easily formatted using the sCalcout record's
<nobr><code>PRINTF(format,variable)</code></nobr> function, which may be
abbreviated as <nobr><code>$P(format,variable)</code></nobr>.

<ul>
<li><P>In the following examples, the
number to be sent to the device is written to the sCalcout record's
<code>A</code> field.

<P><table border>
<tr><th>desired output<th>CALC expression<th>comment
<tr><td>M<em>03</em>;<td>$P("M%02d;",A)<td>doesn't guarantee 2 digits if <code>A</code> &gt; 99<br>
<tr><td>M<em>03</em>;<td>$P("M%02d;",max(0,min(99,A)))<td>enforces limit on <code>A</code>
<tr><td>S<em>1.234000</em><td>$P("S%f",A)<td>default precision and field width
<tr><td>S<em>1.234</em><td>$P("S%.3f",A)<td>controlled precision
<tr><td>S<em> 1.234</em><td>$P("S%6.3f",A)<td>controlled precision and field width
<tr><td>S<em>01.234</em><td>$P("S%06.3f",A)<td>leading zero pad, if needed
</table><P>

<li>Sometimes there are two numbers that have to be sent (e.g., an address which
we'll assume is in the "..formatCmd" record's <code>A</code> field, and a value to be sent to that address,
which we'll assume is in the <code>B</code> field).  Note that the command will be
sent when <i>either</i> <code>A</code> or <code>B</code> is written to (via
channel access).  You can work around this by setting the asyn record's
<code>TMOD</code> field to "NoI/O" when you don't want the command sent.

<P><table border>
<tr><th>desired output<th>CALC expression<th>comment
<tr><td>M<em>03</em>=<em>4.235</em><td>$P("M%02d=",A)+$P("%.3f",B)<td>Note $P()
takes only two arguments.
</table><P>

<li>Sometimes a device's command syntax will require that an integer be split
into separate bytes:
<P><table border>
<tr><th>desired output<th>CALC expression<th>comment
<tr><td>M3=<em>01</em>;M3=<em>74</em><td>$P("M3=%02d",A>>8)+$P("M4=%02d",A&255)<td>A=330, sent as base 10 numbers
<tr><td>M3=<em>01</em>;M3=<em>4A</em><td>$P("M3=%02X",A>>8)+$P("M4=%02X",A&255)<td>A=330, sent as hex numbers
</table><P>

<li>Sometimes the device wants to see text strings, but the software that will
use the device wants to send numbers instead.  For example, you might want to
send the number "1" to open a shutter, and the number "0" to close it, but the
device wants to see strings like "S0 OPEN" and "S0 CLOSE".  In this case, you
can make an array of strings that correspond with numbers by putting the "0"
string in the sCalcout record's <code>AA</code> field, the "1" string in the
<code>BB</code>, etc., and treating the string fields as an array, using the
stringCalc operator "@@" to index the array:

<P><table border>

<tr><th>desired output<th>CALC expression<th>comment

<tr><td>S0 OPEN</em><td>$P("S0 %s",@@A)<td>A addresses the string array:
AA="CLOSE";BB="OPEN"

</table><P>

<li>There is one special character you'll have to watch out for.  The backslash
character "\" will be treated by the asyn record as the beginning of an escape
sequence. To send a single backslash to the device, you must escape it with
another backslash:

<P><table border>

<tr><th>desired output<th>CALC expression<th>comment

<tr><td>\</em><td>"\\"<td>small price to pay for the ability to send unprintable
characters

</table><P>


</ul>

<h3>Formatting unprintable output</h3>

<P>Some devices want to see numbers in their raw, binary form.  Prior to EPICS
version 3.14, there was no widely supported way to pass strings that might
contain embedded ASCII NULL characters from one record to another, so
deviceCmdReply would not have been useable for this class of devices.

<P>But EPICS 3.14 provides an escape-translation service for strings containing
unprintable characters, to put their content into a form that can be transported
in a normal EPICS string.  This allows us to load such strings into a database,
send them via channel access, autosave them, etc.  For purposes here, the
service is implemented by the pair of functions
<code>dbTranslateEscape()</code>, which produces raw binary from a string
containing escape sequences, and <code>epicsStrSnPrintEscaped()</code>, which
does the opposite.  (See the <i>EPICS Application Developer's Guide</i> for more
information.)

<P>In the following tables, a one-byte binary value will be represented by
<em>&lt;n&gt;</em>

<ul> <li>In the simplest case of sending fixed binary numbers, you can encode
them as escape sequences. 

<P><table border>

<tr><th>desired output<th>CALC expression<th>comment

<tr><td><em>&lt;2&gt;</em>#<em>&lt;254&gt;</em><td>"\x02#\xfe"<td>using hex
escape sequences

<tr><td><em>&lt;2&gt;</em>#<em>&lt;254&gt;</em><td>"\002#\376"<td>using octal
escape sequences

</table><P>

<li><P>To embed variable binary numbers into an output string, you can use the
sCalcout record's <nobr><code>WRITE(format,variable)</code></nobr> command,
which may be abbreviated as <nobr><code>$W(format,variable)</code></nobr>.  This
function will encode its result as an escaped string, for transmission to the
asyn record.  The asyn record will translate the string into its raw, binary
form before sending it to the device.

<blockquote>The format-indicator characters used with <code>WRITE()</code> are
intended to be familiar from experience you may have had with the standard C
library's  <code>printf()</code> function, but they're used here to specify how
<i>binary</i> numbers will be encoded, so any field-width or precision
specifications will be ignored.</blockquote>

<P><table border>

<tr>
<th>desired output
<th>CALC expression
<th>comment

<tr>
<td><em>&lt;2&gt;</em>
<td><nobr>$W("%c",A)</nobr>
<td>encode the value of the sCalcout
record's A field as a one-byte integer

<tr>
<td><em>&lt;2&gt;</em>#<em>&lt;254&gt;</em>
<td><nobr>$W("%hd",A)+"#"+$W("%hd",B)</nobr>
<td>encode A and B as two-byte integers

<tr>
<td><em>&lt;2&gt;</em>
<td><nobr>$W("%d",A)</nobr>
<td>encode A as a four-byte
integer

<tr>
<td><em>&lt;2.1&gt;</em>
<td><nobr>$W("%f",A)</nobr>
<td>encode A as a four-byte float

<tr>
<td><em>&lt;2.1&gt;</em>
<td><nobr>$W("%lf",A)</nobr>
<td>encode A as an eight-byte float

</table><P>

<li>Some devices require a checksum (or CRC) appended to the command string,
and will ignore the command if the checksum doesn't have the correct
value.  The checksum is calculated from (part of) the command string,
and typically must be appended to it.  The sCalcout record supports a small
number of checksums, and each type comes in two flavors: a function that
calculates the checksum, from a supplied string, and returns it; and
a function that appends the checksum to the supplied string, and returns
the string. 


<P><table border>

<tr>
<th>desired output
<th>CALC expression
<th>comment

<tr>
<td><nobr><em>&lt;2&gt;&lt;10&gt;&lt;3&gt;&lt;XOR checksum&gt;</em></nobr>
<td><nobr>ADD_XOR8("\002"+$W("%c",A)+"\003")</nobr>
<td>append XOR8 checksum to string

<tr>
<td><nobr><em>&lt;2&gt;&lt;10&gt;&lt;3&gt;&lt;modbus checksum&gt;</em></nobr>
<td><nobr>MODBUS("\002"+$W("%c",A)+"\003")</nobr>
<td>append modbus/RTU CRC to string

</table><P>

</ul>

<h3>Parsing printable input strings</h3>

When the asyn record has received a reply from the device, it will cause the
"...parseReply" sCalcout record to process.  The first thing the sCalcout record
will do is retrieve the reply string from the asyn record's <code>AINP</code>
field, and put it in the sCalcout record's <code>AA</code> field.  Our job here,
then, is simply to parse the content of the <code>AA</code> field.  Parsing the
reply from a device generally requires two operations: we have to specify the
part of the reply that contains the information of interest; and we have to
convert that information to the desired form.

<ul> <li>In the simplest possible case, the sCalcout record will both find and
convert for us. The function <nobr><code>INT(string)</code></nobr> searches
<code>string</code> for the first thing that looks like an integer number, and
returns the value of that number. Similarly, the function
<nobr><code>DBL(string)</code></nobr> searches <code>string</code> for the first
thing that looks like a floating point number, and returns its value.

<P><table border>
<tr><th>reply string<th>CALC expression<th>comment
<tr><td>VALUE=12<td>INT(AA)<td>convert to integer
<tr><td>VALUE=1.23<td>DBL(AA)<td>convert to double-precision number
</table><P>

<li>But that only works if the number we want really is the first thing that looks like
a number.  In a more complicated case, we'll have to wade through some uninteresting stuff to get to the number.  If the uninteresting stuff is of fixed length, we can just move a specified number of characters into the string before converting:

<P><table border>
<tr><th>reply string<th>CALC expression<th>comment
<tr><td>VALUE1=1.23<td>INT(AA[7,-1])<td>move past "VALUE1=" and convert to float
<tr><td>VALUE1=1.23<td>$S(AA,"%*7c%f")<td>skip 7 characters and convert to float
</table><P>

<blockquote>See the sCalcout record documentation for more information on the
substring operator "<code>[]</code>".  For purposes here, the syntax is
<code>[&lt;start&gt;,&lt;end&gt;]</code>.  If <code>&lt;start&gt;</code> is a
number, it indicates the number of bytes to skip. <code>&lt;end&gt;</code>
 will always be <nobr>-1</nobr> in this documentation. <P>
</blockquote>

<li>If the uninteresting stuff is not of fixed length, but the variable-length
part of it ends with some fixed string (or even the n<sup>th</sup> instance of
some fixed string), we can skip to the <em>interesting stuff</em> as follows:

<P><table border>
<tr><th>reply string<th>CALC expression<th>comment
<tr><td>REG237=<em>1.23</em><td>DBL(AA["=",-1])<td>move past "=" and convert to double
<tr><td>reg:2 <em>1.23</em><td>$S(AA,"%*s %f)<td>move past <nobr>" "</nobr> and convert to float
<tr><td><nobr>R1=1.23 R32=R<em>7</em></nobr><td><nobr>INT(AA["=",-1]["=",-1])</nobr><td>move past two "=" characters, find an integer
</table><P>

<blockquote>In examples above, we've used the substring operator 
"<code>[&lt;start&gt;,&lt;end&gt;]</code>"  with a string-valued first argument
-- a pattern string -- and the usual second argument of -1.  If the pattern is
found, the result of the operation is the substring beginning just after the
pattern, and continuing to the end of string.  (If the pattern occurs more than
once, only the first instance counts.)<P> </blockquote>

</ul>

<h3>Parsing unprintable input</h3>

<P>Parsing unprintable input poses a different sort of problem than parsing
printable input.  The strategy is still pretty simple: move to the interesting
stuff and convert it.  But moving to the interesting stuff might be complicated
by escape sequences in the preceding bytes.  I'll first discuss moving as a
separate problem, and then worry about converting it.

<ul> <li>In the simplest case, the bytes we have to move through are fixed, and
we can use the <code>[]</code> operator, as in the previous section.  If the
fixed bytes include escape sequences, we just treat them as plain text for the
purpose of counting characters, or of recognizing patterns.  In the following
examples, &lt;2&gt; represents an unprintable byte whose binary value is 2:

<P><table border>

<tr><th>actual reply string<th><nobr>reply string as we see
it</nobr><th>expression<th>comment

<tr><td>&lt;2&gt;&lt;target&gt;
<td>\002&lt;target&gt;
<td>AA[4,-1]<td>skip one
actual byte, encoded as a four-byte escape sequence

<tr><td>&lt;2&gt;abc&lt;3&gt;&lt;target&gt;
<td>\002abc\003&lt;target&gt;
<td><nobr>AA[11,-1]</nobr><td>skip five actual bytes, encoded as an 11-byte
escape sequence...OR...

<tr><td>&lt;2&gt;abc&lt;3&gt;&lt;target&gt;
<td>\002abc\003&lt;target&gt;
<td><nobr>AA["\003",-1]</nobr><td>just
find the "\003" escape sequence

<tr>
</table><P>

<li>If the bytes we have to move through to get to the interesting stuff include
escape sequences that are <i>not</i> fixed, we can't just skip over a specific
number of bytes, because different byte values generally will be encoded as
escape sequences of different lengths.  For example, the byte value 10 will be
encoded as "\n"; 14 will be encoded as "\016"; and 71 will be encoded as "G". 

<P>Thus, in this case we can't count bytes in the escaped string to find the
interesting stuff, and an expression like
<nobr>"<code>AA[&lt;number&gt;,-1]</code>"</nobr> will not work.  We may still
be able to use an expression like
<nobr><code>AA[&lt;string&gt;,-1]</code></nobr> to pattern match the
<nobr>bytes</nobr> immediately preceding the interesting stuff, but only if
the pattern is fixed and cannot also occur earlier in the reply string.

<P><table border>

<tr><th>actual reply string<th><nobr>reply string as we see
it</nobr><th>expression<th>comment

<tr>
<td>&lt;?&gt;abc&lt;3&gt;&lt;target&gt;
<td><nobr>&lt;? bytes&gt;abc\003&lt;target&gt;</nobr>
<td><nobr>AA["abc\003",-1]</nobr><td>find the "abc\003" escape sequence

<tr>
</table><P>

<li>In the worst case (that this software can handle), the bytes we have to move
through to get to the interesting stuff are not fixed, and don't end with a
sequence we can pattern match, but the number of bytes is known.  In this case,
we must count bytes in the raw, binary string.  Fortunately, the
<nobr><code>READ(string,format)</code></nobr> function can do this, because it converts the string from
which it reads into binary form before parsing it, and because it permits a
conversion indicator whose assignment is suppressed.

<P><table border>

<tr><th>actual reply string<th><nobr>reply string as we see
it</nobr><th>expression<th>comment

<tr>
<td>&lt;n&gt;&lt;target&gt;
<td><nobr>&lt;N bytes&gt;&lt;target&gt;</nobr>
<td><nobr>READ(AA,"%*Nc...")</nobr><td>skip N bytes in raw string

<tr>
</table><P>

</ul>

<P>So now we have the tools to do some actual conversions.  We'll use the
<nobr><code>READ(string, format)</code></nobr> function (which may be
abbreviated as <nobr><code>$R(...)</code></nobr>) to convert. As
before, the <em>interesting stuff</em> is marked with color.:

<ul>
<li>The simplest case:
<P><table border>

<tr><th>actual reply string<th><nobr>reply string as we see
it</nobr><th>expression<th>comment

<tr>
<td>&lt;2&gt;<em>&lt;6&gt;</em>
<td>\002<em>\006</em>
<td>READ(AA[4,-1],"%c")
<td>skip four-byte escape sequence, read 8-bit integer

<tr>
<td>&lt;2&gt;<em>&lt;7&gt;&lt;2&gt;</em>
<td>\002<em>\a\002</em>
<td>READ(AA[4,-1],"%hd")
<td>skip four-byte escape sequence, read 16-bit integer

<tr>
<td>&lt;2&gt;<em>&lt;5&gt;&lt;1&gt;&lt;3&gt;&lt;4&gt;</em>
<td>\002\005\001\003\004
<td>READ(AA[4,-1],"%d")
<td>skip four-byte escape sequence, read 32-bit integer

<tr><td>&lt;2&gt;abc&lt;3&gt;<em>&lt;10&gt;</em>
<td>\002abc\003\n
<td><nobr>AA[11,-1]</nobr><td>skip 11-byte escape sequence, read 8-bit integer

<tr><td>&lt;2&gt;abc&lt;3&gt;<em>&lt;101&gt;&lt;5&gt;</em>
<td>\002abc\003A\005
<td><nobr>READ(AA["\003",-1],"%hd")</nobr><td>find "\003", read 16-bit integer

<tr>
<td>&lt;?&gt;abc&lt;3&gt;<em>&lt;5&gt;</em>
<td><nobr>&lt;? bytes&gt;abc\003<em>\005</em></nobr>
<td><nobr>READ(AA["abc\003",-1],"%c")</nobr><td>find "abc\003", read 8-bit integer


<tr>
<td>&lt;3 bytes&gt;<em>&lt;4&gt;</em>
<td><nobr>&lt;? bytes&gt;<em>\004</em></nobr>
<td><nobr>READ(AA,"%*3c%c")</nobr>
<td>skip 3 bytes in raw string, read 8-bit integer


</table><P>
</ul>


</body>

</html>
