/*---------------------------------------------------------------------------
  NeXus - Neutron & X-ray Common Data Format
  
  Application Program Interface Header File
  
  Copyright (C) 2000-2007 Mark Koennecke, Uwe Filges
  
  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 2 of the License, or (at your option) any later version.
 
  This library is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  Lesser General Public License for more details.
 
  You should have received a copy of the GNU Lesser General Public
  License along with this library; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 
  For further information, see <http://www.nexusformat.org>
  
  $Id: napi.h,v 1.1 2009-08-12 14:29:40 rivers Exp $

 ----------------------------------------------------------------------------*/
/** \file 
 * Documentation for the NeXus-API version 4.2
 * 2000-2007, the NeXus group
 * \defgroup c_main C API 
 * \defgroup c_types Data Types
 * \ingroup c_main
 * \defgroup c_init General Initialisation and shutdown
 * \ingroup c_main
 * \defgroup c_readwrite Reading and Writing
 * \ingroup c_main
 * \defgroup c_metadata Meta data routines
 * \ingroup c_main
 * \defgroup c_linking Linking and Group hierarchy
 * \ingroup c_main
 * \defgroup c_memory Memory allocation
 * \ingroup c_main
 * \defgroup c_external External linking
 * \ingroup c_main
 * \defgroup cpp_main C++ API
 */
  
#ifndef NEXUSAPI
#define NEXUSAPI

/* NeXus HDF45 */
#define NEXUS_VERSION   "4.2.0"                /* major.minor.patch */

#define CONSTCHAR       const char

#ifdef _MSC_VER
#define snprintf nxisnprintf
extern int nxisnprintf(char* buffer, int len, const char* format, ... );
#endif /* _MSC_VER */

typedef void* NXhandle;         /* really a pointer to a NexusFile structure */
typedef int NXstatus;
typedef char NXname[128];

/* 
 * Any new NXaccess_mode options should be numbered in 2^n format 
 * (8, 16, 32, etc) so that they can be bit masked and tested easily.
 *
 * To test older non bit masked options (values below 8) use e.g.
 *
 *       if ( (mode & NXACCMASK_REMOVEFLAGS) == NXACC_CREATE )
 *
 * To test new (>=8) options just use normal bit masking e.g.
 * 
 *       if ( mode & NXACC_NOSTRIP )
 *
 */
#define NXACCMASK_REMOVEFLAGS (0x7) /* bit mask to remove higher flag options */

/** \enum NXaccess_mode 
 * NeXus file access codes.
 * \li NXACC_READ read-only
 * \li NXACC_RDWR open an existing file for reading and writing.
 * \li NXACC_CREATE create a NeXus HDF-4 file
 * \li NXACC_CREATE4 create a NeXus HDF-4 file
 * \li NXACC_CREATE5 create a NeXus HDF-5 file.
 * \li NXACC_CREATEXML create a NeXus XML file.
 */
typedef enum {NXACC_READ=1, NXACC_RDWR=2, NXACC_CREATE=3, NXACC_CREATE4=4, 
	      NXACC_CREATE5=5, NXACC_CREATEXML=6, NXACC_TABLE=8, NXACC_NOSTRIP=128} NXaccess_mode;

/**
 * A combination of options from #NXaccess_mode
 */
typedef int NXaccess;

typedef struct {
                char *iname;
                int   type;
               }info_type, *pinfo;  
 
#define NX_OK 1
#define NX_ERROR 0
#define NX_EOD -1

#define NX_UNLIMITED -1

#define NX_MAXRANK 32
#define NX_MAXNAMELEN 64


/** \var NeXus data types
 * \ingroup c_types
 * \li NX_FLOAT32     32 bit float
 * \li NX_FLOAT64     64 nit float == double
 * \li NX_INT8        8 bit integer == byte
 * \li NX_UINT8       8 bit unsigned integer
 * \li NX_INT16       16 bit integer
 * \li NX_UINT16      16 bit unsigned integer
 * \li NX_INT32       32 bit integer
 * \li NX_UINT32      32 bit unsigned integer
 * \li NX_CHAR        8 bit character
 * \li NX_BINARY      lump of binary data == NX_UINT8
*/
/*--------------------------------------------------------------------------*/ 

/* Map NeXus to HDF types */
#define NX_FLOAT32   5
#define NX_FLOAT64   6
#define NX_INT8     20  
#define NX_UINT8    21
#define NX_BOOLEAN NX_UINT
#define NX_INT16    22  
#define NX_UINT16   23
#define NX_INT32    24
#define NX_UINT32   25
#define NX_INT64    26
#define NX_UINT64   27
#define NX_CHAR      4
#define NX_BINARY   21

/* Map NeXus compression methods to HDF compression methods */
#define NX_COMP_NONE 100
#define NX_COMP_LZW 200
#define NX_COMP_RLE 300
#define NX_COMP_HUF 400  

/* to test for these we use ((value / 100) == NX_COMP_LZW) */
#define NX_COMP_LZW_LVL0 (100*NX_COMP_LZW + 0)
#define NX_COMP_LZW_LVL1 (100*NX_COMP_LZW + 1)
#define NX_COMP_LZW_LVL2 (100*NX_COMP_LZW + 2)
#define NX_COMP_LZW_LVL3 (100*NX_COMP_LZW + 3)
#define NX_COMP_LZW_LVL4 (100*NX_COMP_LZW + 4)
#define NX_COMP_LZW_LVL5 (100*NX_COMP_LZW + 5)
#define NX_COMP_LZW_LVL6 (100*NX_COMP_LZW + 6)
#define NX_COMP_LZW_LVL7 (100*NX_COMP_LZW + 7)
#define NX_COMP_LZW_LVL8 (100*NX_COMP_LZW + 8)
#define NX_COMP_LZW_LVL9 (100*NX_COMP_LZW + 9)

typedef struct {
                long iTag;          /* HDF4 variable */
                long iRef;          /* HDF4 variable */
                char targetPath[1024]; /* path to item to link */
                int linkType;          /* HDF5: 0 for group link, 1 for SDS link */
               } NXlink;

#define NXMAXSTACK 50

#define CONCAT(__a,__b) __a##__b        /* token concatenation */

#    ifdef __VMS
#        define MANGLE(__arg)	__arg 
#    else
#        define MANGLE(__arg)   CONCAT(__arg,_)
#    endif

#    define NXopen              MANGLE(nxiopen)
#    define NXclose             MANGLE(nxiclose)
#    define NXmakegroup         MANGLE(nximakegroup)
#    define NXopengroup         MANGLE(nxiopengroup)
#    define NXopenpath          MANGLE(nxiopenpath)
#    define NXopengrouppath     MANGLE(nxiopengrouppath)
#    define NXclosegroup        MANGLE(nxiclosegroup)
#    define NXmakedata          MANGLE(nximakedata)
#    define NXcompmakedata      MANGLE(nxicompmakedata)
#    define NXcompress          MANGLE(nxicompress)
#    define NXopendata          MANGLE(nxiopendata)
#    define NXclosedata         MANGLE(nxiclosedata)
#    define NXputdata           MANGLE(nxiputdata)
#    define NXputslab           MANGLE(nxiputslab)
#    define NXputattr           MANGLE(nxiputattr)
#    define NXgetdataID         MANGLE(nxigetdataid)
#    define NXmakelink          MANGLE(nximakelink)
#    define NXmakenamedlink     MANGLE(nximakenamedlink)
#    define NXopensourcegroup   MANGLE(nxiopensourcegroup)
#    define NXmalloc            MANGLE(nximalloc)
#    define NXfree              MANGLE(nxifree)
#    define NXflush             MANGLE(nxiflush)

#    define NXgetinfo           MANGLE(nxigetinfo)
#    define NXgetrawinfo        MANGLE(nxigetrawinfo)
#    define NXgetnextentry      MANGLE(nxigetnextentry)
#    define NXgetdata           MANGLE(nxigetdata)

#    define NXgetslab           MANGLE(nxigetslab)
#    define NXgetnextattr       MANGLE(nxigetnextattr)
#    define NXgetattr           MANGLE(nxigetattr)
#    define NXgetattrinfo       MANGLE(nxigetattrinfo)
#    define NXgetgroupID        MANGLE(nxigetgroupid)
#    define NXgetgroupinfo      MANGLE(nxigetgroupinfo)
#    define NXsameID            MANGLE(nxisameid)
#    define NXinitgroupdir      MANGLE(nxiinitgroupdir)
#    define NXinitattrdir       MANGLE(nxiinitattrdir)
#    define NXsetnumberformat   MANGLE(nxisetnumberformat)
#    define NXsetcache          MANGLE(nxisetcache)
#    define NXinquirefile       MANGLE(nxiinquirefile) 
#    define NXisexternalgroup   MANGLE(nxiisexternalgroup)
#    define NXlinkexternal      MANGLE(nxilinkexternal)

/* 
 * FORTRAN helpers - for NeXus internal use only 
 */
#    define NXfopen             MANGLE(nxifopen)
#    define NXfclose            MANGLE(nxifclose)
#    define NXfflush            MANGLE(nxifflush)
#    define NXfmakedata         MANGLE(nxifmakedata)
#    define NXfcompmakedata     MANGLE(nxifcompmakedata)
#    define NXfcompress         MANGLE(nxifcompress)
#    define NXfputattr          MANGLE(nxifputattr)


/* 
 * Standard interface 
 *
 * Functions added here are not automatically exported from 
 * a shared library/dll - the symbol name must also be added
 * to the file   src/nexus_symbols.txt
 * 
 */

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */
  /** 
   * Open a NeXus file.
   * NXopen honours full path file names. But it also searches 
   * for files in all the paths given in the NX_LOAD_PATH environment variable. 
   * NX_LOAD_PATH is supposed to hold a list of path string separated by the platform 
   * specific path separator. For unix this is the : , for DOS the ; . Please note 
   * that crashing on an open NeXus file will result in corrupted data. Only after a NXclose 
   * or a NXflush will the data file be valid. 
   * \param filename The name of the file to open
   * \param access_method The file access method. This can be:
   * \li NXACC__READ read access 
   * \li NXACC_RDWR read write access 
   * \li NXACC_CREATE, NXACC_CREATE4 create a new HDF-4 NeXus file
   * \li NXACC_CREATE5 create a new HDF-5 NeXus file
   * \li NXACC_CREATEXML create an XML NeXus file. 
   * see #NXaccess_mode
   * Support for HDF-4 is deprecated.
   * \param pHandle A file handle which will be initialized upon successfull completeion of NXopen.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXopen(CONSTCHAR * filename, NXaccess access_method, NXhandle* pHandle);

  /**
   * close a NeXus file
   * \param pHandle A NeXus file handle as returned from NXopen. pHandle is invalid after this 
   * call.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXclose(NXhandle* pHandle);

  /**
   * flush data to disk
   * \param pHandle A NeXus file handle as initialized by NXopen. 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXflush(NXhandle* pHandle);

  /**
   * NeXus groups are NeXus way of structuring information into a hierarchy. 
   * This function creates a group but does not open it.
   * \param handle A NeXus file handle as initialized NXopen. 
   * \param name The name of the group
   * \param NXclass the class name of the group. Should start with the prefix NX
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXmakegroup (NXhandle handle, CONSTCHAR *name, CONSTCHAR* NXclass);

  /**
   * Step into a group. All further access will be within the opened group.
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param name The name of the group
   * \param NXclass the class name of the group. Should start with the prefix NX
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXopengroup (NXhandle handle, CONSTCHAR *name, CONSTCHAR* NXclass);

  /**
   * Open the NeXus object with the path specified
   * \param handle A NeXus file handle as returned from NXopen. 
   * \param path A unix like path string to a NeXus group or dataset. The path string 
   * is a list of group names and SDS names separated with / (slash). 
   * Example: /entry1/sample/name 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXopenpath (NXhandle handle, CONSTCHAR *path);

  /**
   * Opens the group in which the NeXus object with the specified path exists
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param path A unix like path string to a NeXus group or dataset. The path string 
   * is a list of group names and SDS names separated with / (slash). 
   * Example: /entry1/sample/name 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXopengrouppath (NXhandle handle, CONSTCHAR *path);

  /**
   * Closes the currently open group and steps one step down in the NeXus file 
   * hierarchy.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXclosegroup(NXhandle handle);

  /**
   * Create a multi dimensional data array or dataset. The dataset is NOT opened. 
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param label The name of the dataset
   * \param datatype The data type of this data set. 
   * \param rank The number of dimensions this dataset is going to have
   * \param dim An array of size rank holding the size of the dataset in each dimension. The first dimension 
   * can be NX_UNLIMITED. Data can be appended to such a dimension using NXputslab. 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXmakedata (NXhandle handle, CONSTCHAR* label, int datatype, int rank, int dim[]);

  /**
   * Create a compressed dataset. The dataset is NOT opened. Data from this set will automatically be compressed when 
   * writing and decompressed on reading. 
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param label The name of the dataset
   * \param datatype The data type of this data set. 
   * \param rank The number of dimensions this dataset is going to have
   * \param comp_typ The compression scheme to use. Possible values:
   * \li NX_COMP_NONE no compression 
   * \li NX_COMP_LZW lossless Lempel Ziv Welch compression (recommended)
   * \li NX_COMP_RLE run length encoding (only HDF-4)
   * \li NX_COMP_HUF Huffmann encoding (only HDF-4)
   * \param dim An array of size rank holding the size of the dataset in each dimension. The first dimension 
   * can be NX_UNLIMITED. Data can be appended to such a dimension using NXputslab. 
   * \param bufsize The dimensions of the subset of the data which usually be writen in one go. 
   * This is a parameter used by HDF for performance optimisations. If you write your data in one go, this 
   * should be the same as the data dimension. If you write it in slabs, this is your preferred slab size. 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXcompmakedata (NXhandle handle, CONSTCHAR* label, int datatype, int rank, int dim[], int comp_typ, int bufsize[]);

  /**
   * Switch compression on. This routine is superseeded by NXcompmakedata and thus 
   * is deprecated.
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param compr_type The compression scheme to use. Possible values:
   * \li NX_COMP_NONE no compression 
   * \li NX_COMP_LZW lossless Lempel Ziv Welch compression (recommended)
   * \li NX_COMP_RLE run length encoding (only HDF-4)
   * \li NX_COMP_HUF Huffmann encoding (only HDF-4)
   * \ingroup c_init
   */
extern  NXstatus  NXcompress (NXhandle handle, int compr_type);

  /**
   * Open access to a dataset. After this call it is possible to write and read data or 
   * attributes to and from the dataset.
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param label The name of the dataset
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXopendata (NXhandle handle, CONSTCHAR* label);

  /**
   * Close access to a dataset. 
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXclosedata(NXhandle handle);

  /**
   * Write data to a datset which has previouly been opened with NXopendata. 
   * This writes all the data in one go. Data should be a pointer to a memory 
   * area matching the datatype and dimensions of the dataset.
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param data Pointer to data to write.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_init
   */
extern  NXstatus  NXputdata(NXhandle handle, void* data);

  /**
   * Write an attribute. The kind of attribute written depends on the  
   * poistion in the file: at root level, a global attribute is written, if 
   * agroup is open but no dataset, a group attribute is written, if a dataset is 
   * open, a dataset attribute is written.
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param name The name of the attribute.
   * \param data A pointer to the data to write for the attribute.
   * \param iDataLen The length of the data in data in bytes.
   * \param iType The NeXus data type of the attribute. 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXputattr(NXhandle handle, CONSTCHAR* name, void* data, int iDataLen, int iType);

  /**
   * Write  a subset of a multi dimensional dataset.
   * \param handle A NeXus file handle as initialized by NXopen. 
   * \param data A pointer to a memory area holding the data to write.
   * \param start An array holding the start indices where to start the data subset.
   * \param size An array holding the size of the data subset to write in each dimension.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXputslab(NXhandle handle, void* data, int start[], int size[]);    

  /**
   * Retrieve link data for a dataset. This link data can later on be used to link this 
   * dataset into a different group. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param pLink A link data structure which will be initialized with the required information
   * for linking. 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetdataID(NXhandle handle, NXlink* pLink);

  /**
   * Create a link to the group or dataset described by pLink in the currently open 
   * group. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param pLink A link data structure describing the object to link. This must have been initialized
   * by either a call to NXgetdataID or NXgetgroupID.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXmakelink(NXhandle handle, NXlink* pLink);

  /**
   * Create a link to the group or dataset described by pLink in the currently open 
   * group. But give the linked item a new name.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param newname The new name of the item in the currently open group. 
   * \param pLink A link data structure describing the object to link. This must have been initialized
   * by either a call to NXgetdataID or NXgetgroupID.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_linking
   */
extern  NXstatus  NXmakenamedlink(NXhandle handle, CONSTCHAR* newname, NXlink* pLink);

  /**
   * Open the source group of a linked group or dataset. Returns an error when the item is 
   * not a linked item.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_linking
   */
extern  NXstatus  NXopensourcegroup(NXhandle handle);

  /**
   * Read a complete dataset from the currently open dataset into memory. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param data A pointer to the memory area where to read the data, too. Data must point to a memory 
   * area large enough to accomodate the data read. Otherwise your program may behave in unexpected
   * and unwelcome ways.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetdata(NXhandle handle, void* data);

  /**
   * Retrieve information about the curretly open dataset.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param rank A pointer to an integer which will be filled with the rank of 
   * the dataset.
   * \param dimension An array which will be initialized with the size of the dataset in any of its 
   * dimensions. The array must have at least the size of rank.
   * \param datatype A pointer to an integer which be set to the NeXus data type code for this dataset.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_metadata
   */
extern  NXstatus  NXgetinfo(NXhandle handle, int* rank, int dimension[], int* datatype);

  /**
   * Get the next entry in the currently open group. This is for retrieving infromation about the 
   * content of a NeXus group. In order to search a group NXgetnextentry is called in a loop until 
   * NXgetnextentry returns NX_EOD which indicates that there are no further items in the group.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param name The name of the object
   * \param nxclass The NeXus class name for a group or the string SDS for a dataset.
   * \param datatype The NeXus data type if the item is a SDS. 
   * \return NX_OK on success, NX_ERROR in the case of an error, NX_EOD when there are no more items.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetnextentry(NXhandle handle, NXname name, NXname nxclass, int* datatype);

  /**
   * Read a subset of data from file into memory. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param data A pointer to the memory data where to copy the data too. The pointer must point 
   * to a memory area large enough to accomodate the size of the data read.
   * \param start An array holding the start indices where to start reading the data subset.
   * \param size An array holding the size of the data subset to read for each dimension.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetslab(NXhandle handle, void* data, int start[], int size[]);

  /**
   * Iterate over global, group or dataset attributes depending on the currently open group or 
   * dataset. In order to search attributes multiple calls to NXgetnextattr are performed in a loop 
   * until NXgentnextattr returns NX_EOD which indicates that there are no further attributes.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param pName The name of the attribute
   * \param iLength A pointer to an integer which be set to the length of the attribute data.
   * \param iType A pointer to an integer which be set to the NeXus data type of the attribute.
   * \return NX_OK on success, NX_ERROR in the case of an error, NX_EOD when there are no more items.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetnextattr(NXhandle handle, NXname pName, int *iLength, int *iType);

  /**
   * Read an attribute.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param name The name of the atrribute to read.
   * \param data A pointer to a memory area large enough to hold the attributes value.
   * \param iDataLen The length of data in bytes.
   * \param iType A pointer to an integer which will had been set to the NeXus data type of the attribute.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetattr(NXhandle handle, char* name, void* data, int* iDataLen, int* iType);

  /**
   * Get the count of attributes in the currently open dataset, group or global attributes when at root level.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param no_items A pointer to an integer which be set to the number of attributes available.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetattrinfo(NXhandle handle, int* no_items);

  /**
   * Retrieve link data for the currently open group. This link data can later on be used to link this 
   * group into a different group. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param pLink A link data structure which will be initialized with the required information
   * for linking. 
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetgroupID(NXhandle handle, NXlink* pLink);

  /**
   * Retrieve information about the currently open group.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param no_items A pointer to an integer which will be set to the count 
   *   of group elements available. This is the count of other groups and 
   * data sets in this group.  
   * \param name The name of the group.
   * \param nxclass The NeXus class name of the group.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXgetgroupinfo(NXhandle handle, int* no_items, NXname name, NXname nxclass);

  /**
   * Tests if two link data structures describe the same item.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param pFirstID The first link data for the test.
   * \param pSecondID The second link data structure.
   * \return NX_OK when both link data structures describe the same item, NX_ERROR else.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXsameID(NXhandle handle, NXlink* pFirstID, NXlink* pSecondID);
  /**
   * Resets a pending group search to the start again. To be called in a Nxgetnextentry loop when 
   * a group search has to be restarted.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXinitgroupdir(NXhandle handle);

  /**
   * Resets a pending attribute search to the start again. To be called in a Nxgetnextattr loop when 
   * an attribute search has to be restarted.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXinitattrdir(NXhandle handle);

  /**
   * Sets the format for number printing. This call has only an effect when using the XML physical file 
   * format. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param type The NeXus data type to set the format for.
   * \param format The C-language format string to use for this data type.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXsetnumberformat(NXhandle handle, int type, char *format);

  /**
   * Inquire the filename of the currently open file. FilenameBufferLength of the file name 
   * will be copied into the filename buffer.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param filename The buffer to hold the filename.
   * \param  filenameBufferLength The length of the filename buffer.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXinquirefile(NXhandle handle, char *filename, int filenameBufferLength);

  /**
   * Test if a group is actually pointing to an external file. If so, retrieve the URL of the 
   * external file.
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param name The name of the group to test.
   * \param nxclass The class name of the group to test.
   * \param url A buffer to copy the URL too.
   * \param urlLen The length of the Url buffer. At maximum urlLen bytes will be copied to url.
   * \return NX_OK when the group is pointing to an external file, NX_ERROR else.
   * \ingroup c_readwrite
   */
extern  NXstatus  NXisexternalgroup(NXhandle handle, CONSTCHAR *name, CONSTCHAR *nxclass, char *url, int urlLen); 

  /**
   * Create a link to an external file. This works by creating a NeXus group under the current level in 
   * the hierarchy which actually points to a group in  another file. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param name The name of the group which points to the xternal file.
   * \param nxclass The class name of the group which points to the external file.
   * \param url The URL of the external file. Currently only one URL format is supported: nxfile://path-tofile\#path-in-file. 
   * This consists of two parts: the first part is of course the path to the file. The second part, path-in-file, is the 
   * path to the group in the external file which appears in the first file.  
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_readwrite
   */
extern  NXstatus  NXlinkexternal(NXhandle handle, CONSTCHAR *name, CONSTCHAR *nxclass, CONSTCHAR *url);

  /**
   * Utility function which allocates a suitably sized memory area for the dataset characteristics specified.
   * \param data A pointer to a pointer which will be initialized with a pointer to a suitably sized memory area.
   * \param rank the rank of the data.
   * \param dimensions An array holding the size of the data in each dimension.
   * \param datatype The NeXus data type of the data.
   * \return NX_OK when allocation succeeds, NX_ERROR in the case of an error.   
   * \ingroup c_memory
   */ 
extern  NXstatus  NXmalloc(void** data, int rank, int dimensions[], int datatype);

  /**
   * Utility function to release the memory for data.
   * \param data A pointer to a pointer to free.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_memory
   */
extern  NXstatus  NXfree(void** data);


/*-----------------------------------------------------------------------
    NAPI internals 
------------------------------------------------------------------------*/
  /**
   * Retrieve information about the currently open dataset. In contrast to the main function below, 
   * this function does not try to find out about the size of strings properly. 
   * \param handle A NeXus file handle as initialized by NXopen.
   * \param rank A pointer to an integer which will be filled with the rank of 
   * the dataset.
   * \param dimension An array which will be initialized with the size of the dataset in any of its 
   * dimensions. The array must have at least the size of rank.
   * \param datatype A pointer to an integer which be set to the NeXus data type code for this dataset.
   * \return NX_OK on success, NX_ERROR in the case of an error.   
   * \ingroup c_metadata
   */
extern  NXstatus  NXgetrawinfo(NXhandle handle, int* rank, int dimension[], int* datatype);

/** \typedef void (*ErrFunc)(void *data, char *text)
 * All NeXus error reporting happens through this special function, the 
 * ErrFunc. The NeXus-API allows to replace this error reporting function
 * through a user defined implementation. The default error function prints to stderr. User 
 * defined ones may pop up dialog boxes or whatever.
 * \param data A pointer to some user defined data structure
 * \param text The text of the error message to display. 
 */
typedef void (*ErrFunc)(void *data, char *text);

  /**
   * Set an error function.
   * \param pData A pointer to a user defined data structure which be passed opaquely to 
   * the error display function.
   * \param newErr The new error display function.
   */
extern  void  NXMSetError(void *pData, ErrFunc newErr);

  /**
   * Retrieve the current error display function
   * \return The current erro display function.
   */
extern ErrFunc NXMGetError();

  /**
   * Suppress error reports from the NeXus-API
   */
extern  void  NXMDisableErrorReporting();

  /**
   * Enable error reports from the NeXus-API
   */
extern  void  NXMEnableErrorReporting();


extern void (*NXIReportError)(void *pData,char *text);
extern void *NXpData;
extern char *NXIformatNeXusTime();
extern  NXstatus  NXIprintlink(NXhandle fid, NXlink* link);

/*
  another special function for setting the default cache size for HDF-5
*/
extern  NXstatus  NXsetcache(long newVal);

  typedef struct {
        NXhandle pNexusData;   
        int stripFlag;
        NXstatus ( *nxclose)(NXhandle* pHandle);
        NXstatus ( *nxflush)(NXhandle* pHandle);
        NXstatus ( *nxmakegroup) (NXhandle handle, CONSTCHAR *name, CONSTCHAR* NXclass);
        NXstatus ( *nxopengroup) (NXhandle handle, CONSTCHAR *name, CONSTCHAR* NXclass);
        NXstatus ( *nxclosegroup)(NXhandle handle);
        NXstatus ( *nxmakedata) (NXhandle handle, CONSTCHAR* label, int datatype, int rank, int dim[]);
        NXstatus ( *nxcompmakedata) (NXhandle handle, CONSTCHAR* label, int datatype, int rank, int dim[], int comp_typ, int bufsize[]);
        NXstatus ( *nxcompress) (NXhandle handle, int compr_type);
        NXstatus ( *nxopendata) (NXhandle handle, CONSTCHAR* label);
        NXstatus ( *nxclosedata)(NXhandle handle);
        NXstatus ( *nxputdata)(NXhandle handle, void* data);
        NXstatus ( *nxputattr)(NXhandle handle, CONSTCHAR* name, void* data, int iDataLen, int iType);
        NXstatus ( *nxputslab)(NXhandle handle, void* data, int start[], int size[]);    
        NXstatus ( *nxgetdataID)(NXhandle handle, NXlink* pLink);
        NXstatus ( *nxmakelink)(NXhandle handle, NXlink* pLink);
        NXstatus ( *nxmakenamedlink)(NXhandle handle, CONSTCHAR *newname, NXlink* pLink);
        NXstatus ( *nxgetdata)(NXhandle handle, void* data);
        NXstatus ( *nxgetinfo)(NXhandle handle, int* rank, int dimension[], int* datatype);
        NXstatus ( *nxgetnextentry)(NXhandle handle, NXname name, NXname nxclass, int* datatype);
        NXstatus ( *nxgetslab)(NXhandle handle, void* data, int start[], int size[]);
        NXstatus ( *nxgetnextattr)(NXhandle handle, NXname pName, int *iLength, int *iType);
        NXstatus ( *nxgetattr)(NXhandle handle, char* name, void* data, int* iDataLen, int* iType);
        NXstatus ( *nxgetattrinfo)(NXhandle handle, int* no_items);
        NXstatus ( *nxgetgroupID)(NXhandle handle, NXlink* pLink);
        NXstatus ( *nxgetgroupinfo)(NXhandle handle, int* no_items, NXname name, NXname nxclass);
        NXstatus ( *nxsameID)(NXhandle handle, NXlink* pFirstID, NXlink* pSecondID);
        NXstatus ( *nxinitgroupdir)(NXhandle handle);
        NXstatus ( *nxinitattrdir)(NXhandle handle);
        NXstatus ( *nxsetnumberformat)(NXhandle handle, int type, char *format);
        NXstatus ( *nxprintlink)(NXhandle handle, NXlink* link);
  } NexusFunction, *pNexusFunction;
  /*---------------------*/
  extern long nx_cacheSize;

/* FORTRAN internals */

  extern NXstatus  NXfopen(char * filename, NXaccess* am, 
					NexusFunction* pHandle);
  extern NXstatus  NXfclose (NexusFunction* pHandle);
  extern NXstatus  NXfputattr(NXhandle fid, char *name, void *data, 
                                   int *pDatalen, int *pIType);
  extern NXstatus  NXfcompress(NXhandle fid, int *compr_type);
  extern NXstatus  NXfcompmakedata(NXhandle fid, char *name, 
                int *pDatatype,
		int *pRank, int dimensions[],
                int *compression_type, int chunk[]);
  extern NXstatus  NXfmakedata(NXhandle fid, char *name, int *pDatatype,
		int *pRank, int dimensions[]);
  extern NXstatus  NXfflush(NexusFunction* pHandle);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /*NEXUSAPI*/

