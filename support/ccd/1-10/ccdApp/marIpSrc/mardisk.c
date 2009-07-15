/*********************************************************************
 *
 * mar345: mardisk.c
 * 
 *
 * Copyright by Dr. Claudio Klein, X-Ray Research GmbH.
 *
 * Version:     2.1
 * Date:        14/05/2002
 *
 * Version	Date		Mods
 * 2.1		14/05/2002	Make compatible to VMS
 *
 *********************************************************************/

#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include <math.h>

/*
 * Machine specific calls: __sgi
 */
#ifdef __sgi 
#include <sys/statfs.h>
#include <sys/fstyp.h>
#include <sys/fsid.h>
struct statfs 	disk;
char		fstyp[ FSTYPSZ ];
#endif

/*
 * Machine specific calls: __linux__
 */
#ifdef __linux__
#include <sys/stat.h>
#include <sys/vfs.h>
struct statfs disk;
#endif

/*
 * Machine specific calls: __ppc__
 */
#if (  __ppc__ )
#include <sys/param.h>
#include <sys/mount.h>
struct statfs disk;
#endif

/*
 * Machine specific calls: Sun
 */
#ifdef SUN
#include <sys/types.h>
#include <sys/statvfs.h>
struct statvfs disk;
#endif

/*
 * Machine specific calls: HPUX
 * Added by D. Spruce
 */
#ifdef __hpux
#include <sys/stat.h>
#include <sys/mount.h>
#include <sys/vfs.h>
struct statfs disk;
#endif


/*
 * Machine specific calls: __osf__
 */
#ifdef __osf__
#include <sys/mount.h>
struct statfs disk;
#endif

extern char buf[], str[], statusfile[];

/*
 * Prototypes
 */

float		GetDiskSpace	( char * );
int		IsNFS		( char * );

/******************************************************************
 * Function: GetDiskSpace for UNIX
 ******************************************************************/
float
GetDiskSpace(char *dir)
{
float 		free= 999999.0 ;

	if (strlen(dir) < 1 ) return( free );

#ifdef __sgi
	if(statfs(dir, &disk, sizeof disk, 0) != 0) return( free );
#elif (__linux__ || __hpux || __ppc__ )
	if(statfs(dir, &disk) != 0) return( free );
#elif ( SUN )
	if(statvfs(dir, &disk) != 0) return( free );
#elif __osf__
	if(statfs(dir, &disk, sizeof disk )  != 0 )return( free ); 
#endif

#if ( __vms)	
	/* Don't do any check ... */
	return ( free );
#elif ( __osf__ )
	free = (float)disk.f_bavail * (float)disk.f_fsize / 1024000.;
#else
	if(disk.f_blocks > 0)
		free = (float)disk.f_bfree * (float)disk.f_bsize / 1024000. ;
#endif

	return( free );
}

/******************************************************************
 * Function: IsNFS
 ******************************************************************/
int
IsNFS(char *dir)
{
int	i,is_nfs = 0;
extern int	debug;

	if (strlen(dir) < 1 ) return( is_nfs );

#ifdef __sgi 
	if(statfs(dir, &disk, sizeof disk, 0) != 0) return( is_nfs );
	i= sysfs( GETFSTYP, disk.f_fstyp, fstyp );

	/* 
	 * Possible filesystems are: efs, nfs, nfs2, nfs3, proc, socket,
	 * specfs, fd, namefs, fifofs, dos, iso9660, cdfs, hfs, autofs,
	 * lofs, xfs
	 */
	if ( strstr( fstyp, "nfs" ) ) 
			is_nfs=1;

#ifdef DEBUG1
	printf("mar345: >>>%s<<< fs-type=%s (0x%x)\n",dir,fstyp,disk.f_fstyp);
#endif

#elif (__linux__ )

	/* NFS id is 0x6969, EFS id is 0xEF53 */
	if(statfs(dir, &disk) != 0) return( is_nfs );
	
	if ( disk.f_type == 0x6969 ) is_nfs = 1;

#ifdef DEBUG
if (debug)
	printf("mar345: >>>%s<<< fs-type=0x%x\n",dir,disk.f_type);
#endif

#elif __hpux

	/* NFS id is MOUNT_NFS (=1), UFS id is MOUNT_UFS (=0) */
	if(statfs(dir, &disk) != 0) return( is_nfs );
	
	if ( disk.f_type == MOUNT_NFS ) is_nfs = 1;

#elif SUN
	/* NFS id is "NFS", UFS is "DUFST" */
	if(statvfs(dir, &disk) != 0) return( is_nfs );
	if ( strstr( disk.f_basetype, "NFS" ) ) 
			is_nfs=1;

#ifdef DEBUG
if (debug)
	printf("mar345: >>>%s<<< fs-type=0x%x\n",dir,disk.f_type);
#endif

#elif __osf__
	if(statfs(dir, &disk, sizeof disk )  != 0 )return( is_nfs ); 
#endif

		
	return(is_nfs);
}
