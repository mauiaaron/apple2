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

#ifdef ANDROID
#warning do newer androids dream of gzbuffer() ?
// Ugh, why is gzbuffer() symbol not part of zlib.h on Android?  I guess maybe we should just go with the published
// default buffer size?  Yay, GJ Goog!
#define CHUNK 8192
#else
#define CHUNK 16384
#endif

/* report a zlib or i/o error */
static const char* const _gzerr(gzFile gzf) {
    if (gzf == NULL) {
        return NULL;
    }
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
const char *zlib_deflate(const char* const src, const int expected_bytecount) {
    unsigned char buf[CHUNK];
    int fd = -1;
    gzFile gzdest = NULL;

    if (src == NULL) {
        return NULL;
    }

    int bytecount = 0;
    char *err = NULL;
    char dst[PATH_MAX] = { 0 };
    do {
        TEMP_FAILURE_RETRY(fd = open(src, O_RDONLY));
        if (fd == -1) {
            ERRLOG("Cannot open file '%s' for reading", src);
            err = ZERR_DEFLATE_OPEN_SOURCE;
            break;
        }

        snprintf(dst, PATH_MAX-1, "%s%s", src, ".gz");

        gzdest = gzopen(dst, "wb");
        if (gzdest == NULL) {
            ERRLOG("Cannot open file '%s' for writing", dst);
            break;
        }

#if !defined(ANDROID)
        if (gzbuffer(gzdest, CHUNK) != Z_OK) {
            ERRLOG("Cannot set bufsize on gz output");
            break;
        }
#endif

        // deflate ...
        do {
            ssize_t buflen = 0;
            TEMP_FAILURE_RETRY(buflen = read(fd, buf, CHUNK));

            if (buflen < 0) {
                ERRLOG("OOPS read ...");
                err = ZERR_DEFLATE_READ_SOURCE;
                break;
            } else if (buflen == 0) {
                break; // DONE
            }

            size_t written = gzwrite(gzdest, buf, buflen);
            if (written < buflen) {
                ERRLOG("OOPS gzwrite ...");
                break;
            }
            bytecount += written;
        } while (1);

    } while (0);

    if (bytecount != expected_bytecount) {
        ERRLOG("OOPS did not write expected_bytecount of %d ... apparently wrote %d", expected_bytecount, bytecount);
        if (gzdest) {
            err = (char *)_gzerr(gzdest);
        }
        if (!err) {
            err = ZERR_UNKNOWN;
        }
    }

    // clean up

    if (fd) {
        TEMP_FAILURE_RETRY(close(fd));
    }

    if (gzdest) {
        gzclose(gzdest);
    }

    if (err) {
        unlink(dst);
    }

    return err;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
const char *zlib_inflate(const char* const src, const int expected_bytecount) {
    gzFile gzsource = NULL;
    int fd = -1;

    if (src == NULL) {
        return NULL;
    }

    int bytecount = 0;
    char *err = NULL;
    char dst[PATH_MAX] = { 0 };
    do {
        gzsource = gzopen(src, "rb");
        if (gzsource == NULL) {
            ERRLOG("Cannot open file '%s' for reading", src);
            break;
        }

        size_t len = strlen(src);
        snprintf(dst, PATH_MAX-1, "%s", src);
        if (! ( (dst[len-3] == '.') && (dst[len-2] == 'g') && (dst[len-1] == 'z') ) ) {
            ERRLOG("Expected filename ending in .gz");
            break;
        }
        dst[len-3] = '\0';

        TEMP_FAILURE_RETRY(fd = open(dst, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR));
        if (fd == -1) {
            err = ZERR_INFLATE_OPEN_DEST;
            ERRLOG("Cannot open file '%s' for writing", dst);
            break;
        }

        // inflate ...
        do {
            unsigned char buf[CHUNK];
            int buflen = gzread(gzsource, buf, CHUNK-1); // hmmm ... valgrind complains of a buffer read overflow in zlib inffast()?
            if (buflen < 0) {
                ERRLOG("OOPS, gzip read ...");
                break;
            } else if (buflen == 0) {
                break; // DONE
            }

            ssize_t idx = 0;
            size_t chunk = buflen;
            do {
                ssize_t outlen = 0;
                TEMP_FAILURE_RETRY(outlen = write(fd, buf+idx, chunk));
                if (outlen <= 0) {
                    break;
                }
                idx += outlen;
                chunk -= outlen;
            } while (idx < buflen);

            if (idx != buflen) {
                ERRLOG("OOPS, write");
                err = ZERR_INFLATE_WRITE_DEST;
                break;
            }

            bytecount += buflen;
        } while (1);

    } while (0);

    if (bytecount != expected_bytecount) {
        ERRLOG("OOPS did not write expected_bytecount of %d ... apparently wrote %d", expected_bytecount, bytecount);
        if (gzsource) {
            err = (char *)_gzerr(gzsource);
        }
        if (!err) {
            err = ZERR_UNKNOWN;
        }
    }

    // clean up

    if (gzsource) {
        gzclose(gzsource);
    }

    if (fd) {
        TEMP_FAILURE_RETRY(fsync(fd));
        TEMP_FAILURE_RETRY(close(fd));
    }

    if (err) {
        unlink(dst);
    }

    return err;
}

