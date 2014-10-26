program versionTest

#include "seq_release.h" /* for conditional compilation in initial (pre-snc) CPP pass */
%%#include <stdlib.h>    /* for exit() */

entry {
/* conditional compilation in SNL code, should not be needed */
    printf("conditional compilation in\n");
#if MAGIC >= 2002000
    printf("seq version is 2.2 or greater\n");
#else
    printf("seq version is 2.1 or less\n");
#endif

/* conditional compilation in escaped C code, MAGIC and directives resolved by initial CPP pass */
%{
#if MAGIC >= 2002000
    printf("seq version is 2.2 or greater\n");
#else
    printf("seq version is 2.1 or less\n");
#endif
}%

/* conditional compilation in escaped C code */
%%/* an #include "seq_release.h" is not needed when testing inside escaped C code */
%%#if MAGIC >= 2002000
%%    printf("seq version is 2.2 or greater\n");
%%#else
%%    printf("seq version is 2.1 or less\n");
%%#endif
}

ss x { state x { when (delay(1)) {exit(0);} exit } }
