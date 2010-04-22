/*
 * Open window to all I/O ports and run
 * application as non-privileged user.
 *
 * Install this executable setuid-root.
 */

/*
 * $Id: startWithIopl3.c,v 1.2 2005-01-05 14:15:37 rivers Exp $
 */

#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/io.h>

int
main (int argc, char **argv)
{
    if (argc < 2) {
        fprintf(stderr, "Usage: %s executable [args ...]\n", argv[0]);
        return 1;
    }

    /*
     * Open the I/O ports
     */
    if (iopl(3) != 0) {
        fprintf(stderr, "Can't open access to I/O ports: %s\n", strerror(errno));
        return 2;
    }

    /*
     * Relinquish super-user status
     */
    setuid(getuid());

    /*
     * Execute the application
     */
    argv++;
    execv(argv[0], argv);
    fprintf(stderr, "Can't execute %s: %s\n", argv[0], strerror(errno));
    return 3;
}
