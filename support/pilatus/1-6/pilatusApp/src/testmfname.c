#include <stdio.h>

#define MAX_FILENAME_LEN 256
char FullFilename[MAX_FILENAME_LEN];
char mfFormat[MAX_FILENAME_LEN];
int mfNumber;
int NImages;

static void makeMultipleFileFormat();

int main(int argc, void **argv) {

    printf("argv[1]=%s\n", argv[1]);
    
    strcpy(FullFilename, argv[1]);
    NImages = atoi(argv[2]);
    makeMultipleFileFormat();
    printf("mfFormat=%s, NImages=%d, mfNumber=%d\n", mfFormat, NImages, mfNumber);
}

static void makeMultipleFileFormat()
{
    /* This function uses the code from camserver */
    char *p, *q;
    int fmt;
    char mfTempFormat[MAX_FILENAME_LEN];
    char mfExtension[10];
    
    /* FullFilename has been built by the caller.
     * Copy to temp */
    strcpy(mfTempFormat, FullFilename);
    p = mfTempFormat + strlen(mfTempFormat) - 5; /* look for extension */
    if ( (q=strrchr(p, '.')) ) {
        strcpy(mfExtension, q);
        *q = '\0';
    } else {
        strcpy(mfExtension, ""); /* default is raw image */
    }
    mfNumber=0;   /* start number */
    fmt=5;        /* format length */
    if ( !(p=strrchr(mfTempFormat, '/')) ) {
        p=mfTempFormat;
    }
    if ( (q=strrchr(p, '_')) ) {
        q++;
        if (isdigit(*q) && isdigit(*(q+1)) && isdigit(*(q+2))) {
            mfNumber=atoi(q);
            fmt=0;
            p=q;
            while(isdigit(*q)) {
                fmt++;
                q++;
            }
            *p='\0';
            if (((fmt<3)  || ((fmt==3) && (NImages>999))) || 
                ((fmt==4) && (NImages>9999))) { 
                fmt=5;
            }
        } else if (*q) {
            strcat(p, "_"); /* force '_' ending */
        }
    } else {
        strcat(p, "_"); /* force '_' ending */
    }
    /* Build the final format string */
    snprintf(mfFormat, sizeof(mfFormat), "%s%%.%dd%s",
                  mfTempFormat, fmt, mfExtension);
}
