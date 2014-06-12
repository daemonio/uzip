/*
 * Unpack zip (uzip)
 * unpack files in zips. This program ~does not~
 * decompress the files, it just extract it.
 * Useful when the zip manager can't let you
 * see the files without the password.
 *
 * $ uzip zipfile.txt
 * Unpacking file1.txt
 * Unpacking file2.txt
 * ....
 *
 * in this example, the txt files are COMPRESSED
 * so you can't read their content, unless if the
 * files were just archived (not compressed).
 *
 * TODO: handle filenames with '/'
 * TODO: handle filenames > MAX_FILENAME
 * Thu Jun 12 15:09:27 BRT 2014
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CENTRAL_HDR_SIZE 46  /* hdr size without file name */
#define LOCAL_HDR_SIZE   30
#define MAX_FILENAME    200
#define OCD_SIZE         20

/* not all the fields */
struct st_central_hdr {
    unsigned int csize, loffset ;
    unsigned int n, m, k;
    char filename[MAX_FILENAME] ;
} ;

/* local header sig */
int sig_local_header[]  = {0x50, 0x4b, 0x03, 0x04} ;
/* end of centrao dir sig (reversed) */
const int sig_end_central_dir[] = {0x06, 0x05, 0x4b, 0x50} ;

void show_help(char *s) {
    printf("** Unpack zip **\n"
           "[use] %s <zip_file_1> [zip file 2] [zip file 3] ..\n",s) ;
    exit(-1);
}

int find_end_central_dir(FILE *zipfile) {
    int j ;
    unsigned char c ;

    j=0;
    fseek(zipfile, -1, SEEK_END) ;
    while(fread(&c, sizeof(unsigned char), 1, zipfile)) {
        /* EOCD sig */
        if(c == sig_end_central_dir[j]) j++ ;
        else j=0 ;

        if(j == 4) {
            return 1;
        }
        /* we hitted the beggening of the file.. exit */
        if(fseek(zipfile, -2, SEEK_CUR) < 0) return 0;
    }
    return 0 ;
}

/* you'll need the zip format table to understand these indexes.
 * ie: 28 is the offset for the filename length
*/
void set_central_hdr_fields(struct st_central_hdr *cc, char *centralbuf) {
    cc->n = *((int *)&centralbuf[28]) ; cc->n &= 0xffff; /* filename length */
    cc->m = *((int *)&centralbuf[30]) ; cc->m &= 0xffff; /* extra field length */
    cc->k = *((int *)&centralbuf[32]) ; cc->k &= 0xffff; /* comment length */
    cc->csize = *((int *)&centralbuf[20]) ;
    cc->loffset = *((int *)&centralbuf[42]) ;
}

void extract_file(struct st_central_hdr *cc, FILE *zipfile) {
    char localbuf[LOCAL_HDR_SIZE] ;
    int n, m ;
    int error=0 ;
    unsigned int written ;
    unsigned char c;

    FILE *out ;

    /* local buffer header */
    fread(localbuf, sizeof(char), LOCAL_HDR_SIZE, zipfile) ;

    /* sanity check */
    for(n=0; n < 4; n++) {
        if(localbuf[n] != sig_local_header[n]) error=1 ;
    }

    /* read filename length again */
    n = *((int *)&localbuf[26]) ; n &= 0xffff;
    /* this maybe diff. from the value from central dir */
    m = *((int *)&localbuf[28]) ; m &= 0xffff;

    /* sanity check */
    if(n != cc->n) error=1;
    /* sanity check */
    if(error==1) {
        printf("[+] Error: Corrupted file?\n") ;
        return ;
    }

    /* Here we open the file and write its bytes.
     * How much we are going to write is cc->csize
     */
    out=fopen(cc->filename, "w+b") ;
    if(out==NULL) {
        printf("[+] Warning: can't open file.\n") ;
        return ;
    }
    printf("Unpacking `%s' (offset: 0x%08x) (csize: 0x%08x)\n",
            cc->filename,
            cc->loffset,
            cc->csize) ;

    /* skip file name and extra fields */
    fseek(zipfile, cc->n + m, SEEK_CUR) ;

    /* read/write 1  byte at a time. Change this
     * for a more eff. IO
     */
    written=0;
    while(written < cc->csize) {
        fread(&c, sizeof(char), 1, zipfile) ;
        fwrite(&c, sizeof(char), 1, out) ;
        written++ ;
    }
    fclose(out) ;
}

int main(int argc, char **argv) {
    FILE *zipfile ;
    int i, j ;
    int nzip ;

    char centralbuf[CENTRAL_HDR_SIZE] ;
    char eocdhdr[OCD_SIZE] ;
    int CD_offset ;

    int CD_nr ; /* number of records */

    struct st_central_hdr *centralvector, *cc ;

    if(argc == 1) show_help(argv[0]) ;

    /* each zip from command line */
    for(nzip=1; nzip < argc; nzip++) {
        zipfile = fopen(argv[nzip], "r+b") ;
        if(zipfile == NULL) {
            printf("[+] Error: Can't open `%s'\n", argv[nzip]) ;
            perror("") ;return -1; 
        }

        /* try to find the EOCD */
        if(find_end_central_dir(zipfile) == 0) {
            puts("[+] Error: Can't find end of central dir (EOCD). Is this a zip file?") ;
            return -1;
        }

        /* read the EOCD */
        fread(&eocdhdr, sizeof(char), OCD_SIZE, zipfile) ;
        CD_offset = *((int *)&eocdhdr[15]) ;
        CD_nr     = *((int *)&eocdhdr[7]) ; CD_nr &= 0xffff ;

        printf("Total files in (`%s'): %d\n", argv[nzip], CD_nr) ;

        /* all files info are inside this vector */
        centralvector = (struct st_central_hdr *)
            malloc(CD_nr * (sizeof(struct st_central_hdr))) ;

        /* iterate thru central dir */
        fseek(zipfile, CD_offset, SEEK_SET) ;
        for(i=0; i < CD_nr; i++) {
            /* read the central dir header */
            fread(&centralbuf, sizeof(char), CENTRAL_HDR_SIZE, zipfile) ;
            cc = &centralvector[i] ;
            /* set the info */
            set_central_hdr_fields(cc, centralbuf) ;
            /* read filename */
            j=fread(&cc->filename, sizeof(char), cc->n, zipfile);
            cc->filename[j]=0 ;
            for(j=0; cc->filename[j] != 0; j++) {
                if(cc->filename[j]=='/') /* TODO: treat directory correctly */
                    cc->filename[j]='_'; /* for now, changing '/' to '_' */
            }
            /* skip to the next file */
            fseek(zipfile, cc->m+cc->k, SEEK_CUR) ;
        }

        /* iterate thru local file */
        for(i=0; i < CD_nr; i++) {
            cc = &centralvector[i] ;
            /* goes to the local file offset  */
            fseek(zipfile, cc->loffset, SEEK_SET) ;
            /* extract it */
            extract_file(cc, zipfile) ;
        }

        free(centralvector) ;
    }
    return 0;
}
