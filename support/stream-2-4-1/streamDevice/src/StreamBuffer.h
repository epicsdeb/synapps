/***************************************************************
* StreamBuffer                                                 *
*                                                              *
* (C) 2005 Dirk Zimoch (dirk.zimoch@psi.ch)                    *
*                                                              *
* This is a buffer class used in StreamDevice for I/O.         *
* Please refer to the HTML files in ../doc/ for a detailed     *
* documentation.                                               *
*                                                              *
* If you do any changes in this file, you are not allowed to   *
* redistribute it any more. If there is a bug or a missing     *
* feature, send me an email and/or your patch. If I accept     *
* your changes, they will go to the next release.              *
*                                                              *
* DISCLAIMER: If this software breaks something or harms       *
* someone, it's your problem.                                  *
*                                                              *
***************************************************************/

#ifndef StreamBuffer_h
#define StreamBuffer_h

#include <string.h>
#include <stdio.h>

#ifndef __GNUC__
#define __attribute__(x)
#endif

class StreamBuffer
{
    char local[64];
    long len;
    long cap;
    long offs;
    char* buffer;

    void grow(long);
    void init(const void*, long);

    void check(long size)
        {if (len+offs+size >= cap) grow(len+size);}

public:
    // Hints:
    // * Any index parameter (long) can be negative
    //   meaning "count from end" (-1 is the last byte)
    // * Any returned char* pointer becomes invalid when
    //   the StreamBuffer is modified.

    StreamBuffer()
        {init(NULL, 0);}

    StreamBuffer(const void*s, long size)
        {init(s, size);}

    StreamBuffer(const char*s)
        {init(s, s?strlen(s):0);}

    StreamBuffer(const StreamBuffer& s)
        {init(s.buffer+s.offs, s.len);}

    StreamBuffer(long size)
        {init(NULL, size);}

    ~StreamBuffer()
        {if (buffer != local) delete buffer;}

    // operator (): get char* pointing to index
    const char* operator()(long index=0) const
        {buffer[offs+len]=0; return buffer+offs+(index<0?index+len:index);}

    char* operator()(long index=0)
        {buffer[offs+len]=0; return buffer+offs+(index<0?index+len:index);}

    // operator []: get byte at index
    char operator[](long index) const
        {return buffer[offs+(index<0?index+len:index)];}

    char& operator[](long index)
        {return buffer[offs+(index<0?index+len:index)];}

    // cast to bool: not empty?
    operator bool() const
        {return len>0;}

    // length: get current data length
    long length() const
        {return len;}

    // capacity: get current max data length (spare one byte for end)
    long capacity() const
        {return cap-1;}

    // end: get pointer to byte after last data byte
    const char* end() const
        {return buffer+offs+len;}

    // clear: set length to 0, don't free or blank memory (fast!)
    StreamBuffer& clear()
        {offs+=len; len=0; return *this;}

    // reserve: reserve size bytes of memory and return
    // pointer to that memory (for copying something to it)
    char* reserve(long size)
        {grow(size); char* p=buffer+len; len+=size; return p;}

    // append: append data at the end of the buffer
    StreamBuffer& append(char c, long count=1)
        {check(count); while(count-->0) buffer[offs+len++]=c; return *this;}

    StreamBuffer& append(const void* s, long size);

    StreamBuffer& append(const char* s)
        {return append(s, s?strlen(s):0);}

    StreamBuffer& append(const StreamBuffer& s)
        {return append(s.buffer+s.offs, s.len);}

    // set: clear buffer and fill with new data
    StreamBuffer& set(const void* s, long size)
        {clear(); return append(s, size);}

    StreamBuffer& set(const char* s)
        {clear(); return append(s, s?strlen(s):0);}

    StreamBuffer& set(const StreamBuffer& s)
        {clear(); return append(s.buffer+s.offs, s.len);}

    // operator =: alias for set
    StreamBuffer& operator=(const char* s)
        {return set(s);}

    StreamBuffer& operator=(const StreamBuffer& s)
        {return set(s);}

    // replace: delete part of buffer (pos/length) and insert new data
    StreamBuffer& replace(
        long pos, long length, const void* s, long size);

    StreamBuffer& replace(long pos, long length, const char* s)
        {return replace(pos, length, s, s?strlen(s):0);}

    StreamBuffer& replace(long pos, long length, const StreamBuffer& s)
        {return replace(pos, length, s.buffer+s.offs, s.len);}

    // remove: delete from start/pos
    StreamBuffer& remove(long pos, long length)
        {return replace(pos, length, NULL, 0);}

    StreamBuffer& remove(long length)
        {if (length>len) length=len;
         offs+=length; len-=length; return *this;}

    // truncate: delete end of buffer
    StreamBuffer& truncate(long pos)
        {return replace(pos, len, NULL, 0);}

    // insert: insert new data into buffer
    StreamBuffer& insert(long pos, const void* s, long size)
        {return replace(pos, 0, s, size);}

    StreamBuffer& insert(long pos, const char* s)
        {return replace(pos, 0, s, s?strlen(s):0);}

    StreamBuffer& insert(long pos, const StreamBuffer& s)
        {return replace(pos, 0, s.buffer+s.offs, s.len);}

    StreamBuffer& insert(long pos, char c)
        {return replace(pos, 0, &c, 1);}

    StreamBuffer& printf(const char* fmt, ...)
        __attribute__ ((format(printf,2,3)));

    // find: get index of data in buffer or -1
    long find(char c, long start=0) const
        {char* p;
         return (p = static_cast<char*>(
            memchr(buffer+offs+(start<0?start+len:start),
                c, start<0?-start:len-start)))?
            p-(buffer+offs) : -1;}

    long find(const void* s, long size, long start=0) const;

    long find(const char* s, long start=0) const
        {return find(s, s?strlen(s):0, start);}

    long int find(const StreamBuffer& s, long start=0) const
        {return find(s.buffer+s.offs, s.len, start);}

    // equals: returns true if first size bytes are equal
    bool equals(const void* s, long size) const
        {return len>=size ? memcmp(buffer+offs, s, size) == 0 : false;}

    // equals: returns true if first string is equal (empty string match)
    bool equals(const char* s) const
        {return len ? strcmp(buffer+offs, s) == 0 : !s || !*s;}

// expand: create copy of StreamBuffer where all nonprintable characters
// are replaced by <xx> with xx being the hex code of the characters
    StreamBuffer expand(long start=0, long length=-1) const;

// dump: debug function, like expand but also show the 'hidden' memory
// before and after the real data. Uses colours.
    StreamBuffer dump() const;
};

#endif
