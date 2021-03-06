/* smaFS utility to reclaim space by history thinning and compression */

#include "reaper.h"

using namespace std;

void printshorthash(unsigned char *hash)
{
    int i;
    for (i = 0; i < 4; ++i) {
        printf("%02X", hash[i]);
    }
}

static void reaper(char * path)
{
    /* find metadata path */
    const char *l = strrchr(path,'/');
    int n;
    char metadatapath[256];
    memset(metadatapath, 0, 256);
    char metadatapathnew[256];
    memset(metadatapathnew, 0, 256);
    char targetpath[256];
    memset(targetpath, 0, 256); 
    
    char storepath[256];
    memset(storepath, 0, 256);
 
    if(l) {
        n = int(l-path) + 1;
        memcpy(storepath, path, n);
        memcpy(&storepath[n], ".store", 6);
        n = (int) (&path[strlen(path)]-l);
        memcpy(targetpath, l+1, n);
        n = strlen(storepath);
        memcpy(metadatapath, storepath, n);
    }
    else {
        memcpy(targetpath, path, strlen(path));
        memcpy(metadatapath, ".store", 6);
        memcpy(storepath, ".store", 6);
        n=6;
    }

    sprintf(&metadatapath[n], "/%s%s","metadata.", targetpath);
    sprintf(metadatapathnew, "%s.new", metadatapath);

    fprintf(stderr,"Metadata path for %s is %s\n",path, metadatapath);
    fprintf(stderr,"New Metadata path for %s is %s\n",path, metadatapathnew);
   
    struct stat stbuf; int status;
    status = lstat(metadatapath, &stbuf);
    if (status == -1) {
        printf("No versions for %s found: %s!\n",path, strerror(errno));
        return;
    }

    printf("Reaper is processing file: %s ...\n\n",path);
    printf("%-10s%-12s%-28s%-10s%-10s  %s  %s\n\n", "|Result|", "|Revision|", "|Timestamp|", "|Size|", "|Checksum|", "|Compressed|", "|Real Name|");
    /* read versions from metadata file */
    FILE *fp = fopen(metadatapath, "r");
    FILE *fpn = fopen(metadatapathnew, "wb");
    char cresult[256];
    memset(cresult, 0, 256);
    
    struct version v;
    n = sizeof(struct version);
    memset(&v,0,n);
    while(fp && !feof(fp)) {
        int result = fread(&v, n, 1, fp);
        if(result == 0) 
            break;
        if(v.isCompressed) {
            sprintf(cresult, "%s", "AC");
        }
        else {
            /* update stuff */
            char ofile[256];
            memset(ofile, 0, 256);
            char nfile[256];
            memset(nfile, 0, 256);
            
            double ratio;

            sprintf(ofile, "%s/%s", storepath, v.vfn);
            sprintf(nfile, "%s.new", ofile);
            compress(ofile,nfile, &ratio);
            unlink(ofile);
            rename(nfile, ofile);
            chown(ofile, getuid(), getgid());  /* fix permissions */
            v.isCompressed = true;
            
            unsigned char *hash = gethash(path);
            if(hash) {
                memcpy(&v.hash,hash,4);
            }
            sprintf(cresult, "%.1g", ratio);
        }
        
        printf("%-10s", cresult);
        printf("%08d    ",v.revision);
        char *s = ctime(&v.stbuf.st_mtime);
        int i = strlen(s)-1; s[i]=0;
        printf("%-28s", s);
        printf("%-10d", (int)v.stbuf.st_size);
        printshorthash(v.hash);
        v.isCompressed == 0 ? printf("    %-8s", "false") : printf("    %-8s", "true");
        printf("      %s  ", v.vfn);        
 
        fwrite(&v,n,1,fpn);
        printf("\n");
 
    }
    
    if(fp) fclose(fp);
    if(fpn) fclose(fpn);
    unlink(metadatapath);
    rename(metadatapathnew, metadatapath);
    chown(metadatapath, getuid(), getgid());  /* fix permissions */

}


static void usage(char *program)
{
    printf("Usage: %s [-c] [-t] files(s)\n", program);
}

int main(int argc, char *argv[])
{

    if(argc < 2) {
        usage(argv[0]);
        exit(EXIT_FAILURE);
    }

    printf("%d %d\n", getuid(), getgid());

    for(int i=1; i < argc; i++) {
        reaper(argv[i]);
    }
    
    return 0;
}

