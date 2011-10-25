/***********************************************
 * osdNfs.c
 * 
 * Realize the Linux specified routines for NFS operation
 *
 * Created by: Zheqiao Geng, gengzq@slac.stanford.edu
 * Created on: Aug. 13, 2010
 * Description: Realize the basic function for NFS mount and dismount
 ***********************************************/
#include "osdNfs.h"

/**
 * Global variables
 */
int save_restoreNFSOK    = 1;  /* for Linux, NFS has been mounted before autosave starts */
int save_restoreIoErrors = 0;  /* for accumulate the IO error numbers, when the number larger than threshold, remount NFS */


/* Note: file system mounting is managed by Linux */
int mountFileSystem(char *uidhost, char *addr, char *path, char *mntpoint)
{
    printf("NFS mounting for solaris is managed by OS\n");
	return(OK);
}

/* Note: file system mounting is managed by Linux */
int dismountFileSystem(char *mntpoint)
{
    printf("Not allowed to dismount for solaris\n");
	return(OK);
}
