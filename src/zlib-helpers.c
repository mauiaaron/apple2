/*
 * Apple // emulator for *nix 
 *
 * This software package is subject to the GNU General Public License
 * version 2 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * THERE ARE NO WARRANTIES WHATSOEVER.
 *
 */

/* zpipe.c: example of proper use of zlib's inflate() and deflate()
   Not copyrighted -- provided to the public domain
   Version 1.4  11 December 2005  Mark Adler */

/* Version history:
   1.0  30 Oct 2004  First version
   1.1   8 Nov 2004  Add void casting for unused return values
                     Use switch statement for inflate() return values
   1.2   9 Nov 2004  Add assertions to document zlib guarantees
   1.3   6 Apr 2005  Remove incorrect assertion in inf()
   1.4  11 Dec 2005  Add hack to avoid MSDOS end-of-line conversions
                     Avoid some compiler warnings for input and output buffers
   ...
   xxx   1 Dec 2013  Added to Apple2ix emulator, replaced assertions with error value returns
   xxx  22 Dec 2013  Switched to gz...() routines for simplicity and to be compatible with gzip
 */

#include "common.h"

#define CHUNK 16384
#define UNKNOWN_ERR 42

/* report a zlib or i/o error */
static const char* const _gzerr(gzFile gzf)
{
    int err_num = 0;
    const char *reason = gzerror(gzf, &err_num);
    if (err_num == Z_ERRNO) {
        return strerror(err_num);
    } else {
        return reason;
    }
}

/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be allocated
   for processing, Z_VERSION_ERROR if the version of zlib.h and the version of
   the library linked do not match, or Z_ERRNO if there is an error reading or
   writing the files. */
const char *def(const char* const src, const int expected_bytecount)
{
    unsigned char buf[CHUNK];
    FILE *source = NULL;
    gzFile gzdest = NULL;

    if (src == NULL) {
        return NULL;
    }

    int bytecount = 0;
    int ret = UNKNOWN_ERR;
    do {
        source = fopen(src, "r");
        if (source == NULL) {
            ERRLOG("Cannot open file '%s' for reading", src);
            break;
        }

        char dst[TEMPSIZE];
        snprintf(dst, TEMPSIZE-1, "%s%s", src, ".gz");

        gzdest = gzopen(dst, "wb");
        if (gzdest == NULL) {
            ERRLOG("Cannot open file '%s' for writing", dst);
            break;
        }

        int err = gzbuffer(gzdest, CHUNK);
        if (err != Z_OK) {
            ERRLOG("Cannot set bufsize on gz output");
            break;
        }

        // deflate ...
        do {
            size_t buflen = fread(buf, 1, CHUNK, source);

            if (ferror(source)) {
                ERRLOG("OOPS fread ...");
                break;
            }

            if (buflen > 0) {
                int written = gzwrite(gzdest, buf, buflen);
                if (written < buflen) {
                    ERRLOG("OOPS gzwrite ...");
                    break;
                }
                bytecount += written;
            }

            if (feof(source)) {
                if (bytecount != expected_bytecount) {
                    ERRLOG("OOPS did not write expected_bytecount of %d ... apparently wrote %d", expected_bytecount, bytecount);
                    ret = UNKNOWN_ERR;
                } else {
                    ret = Z_OK;
                }
                break; // finished deflating
            }
        } while (1);

    } while (0);

    const char *err = NULL;
    if (ret != Z_OK) {
        err = _gzerr(gzdest);
    }

    // clean up

    if (source) {
        fclose(source);
    }

    if (gzdest) {
        gzclose(gzdest);
    }

    return err;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
const char *inf(const char* const src, int *rawcount)
{
    unsigned char buf[CHUNK];
    gzFile gzsource = NULL;
    FILE *dest = NULL;

    if (src == NULL) {
        return NULL;
    }

    if (rawcount) {
        *rawcount = 0;
    }
    int ret = UNKNOWN_ERR;
    do {
        gzsource = gzopen(src, "rb");
        if (gzsource == NULL) {
            ERRLOG("Cannot open file '%s' for reading", src);
            break;
        }

        int len = strlen(src);
        char dst[TEMPSIZE];
        snprintf(dst, TEMPSIZE-1, "%s", src);
        if (! ( (dst[len-3] == '.') && (dst[len-2] == 'g') && (dst[len-1] == 'z') ) ) {
            ERRLOG("Expected filename ending in .gz");
            break;
        }
        dst[len-3] = '\0';

        FILE *dest = fopen(dst, "w+");
        if (dest == NULL) {
            ERRLOG("Cannot open file '%s' for writing", dst);
            break;
        }

        // inflate ...
        do {
            int buflen = gzread(gzsource, buf, CHUNK);
            if (buflen < 0) {
                ERRLOG("OOPS, gzip read ...");
                break;
            }

            if (buflen == 0) {
                ret = Z_OK;
                break; // finished inflating
            }

            if ( (fwrite(buf, 1, buflen, dest) != buflen) || ferror(dest) ) {
                ERRLOG("OOPS fwrite ...");
                break;
            }
            if (rawcount) {
                *rawcount += buflen;
            }
            fflush(dest);
        } while (1);

    } while (0);

    const char *err = NULL;
    if (ret != Z_OK) {
        err = _gzerr(gzsource);
    }

    // clean up

    if (gzsource) {
        gzclose(gzsource);
    }

    if (dest) {
        fflush(dest);
        fclose(dest);
    }

    return err;
}

