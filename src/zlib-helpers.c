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
 */

#include "misc.h"
#include "zlib-helpers.h"

#include <zlib.h>

#define CHUNK 16384
#define UNKNOWN_ERR 42

static int _def(FILE *source, FILE *dest, const int level) {
    int flush;
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate deflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    int ret = deflateInit(&strm, level);
    if (ret != Z_OK) {
        ERRLOG("OOPS deflateInit : %d", ret);
        return ret;
    }

    /* compress until end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)deflateEnd(&strm);
            ERRLOG("OOPS fread ...");
            return Z_ERRNO;
        }
        flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
        strm.next_in = in;

        /* run deflate() on input until output buffer not full, finish
           compression if all of source has been read in */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = deflate(&strm, flush);    /* no bad return value */
            if (ret == Z_STREAM_ERROR) {
                ERRLOG("OOPS deflate : %d", ret);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)deflateEnd(&strm);
                ERRLOG("OOPS fwrite ...");
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);
        if (strm.avail_in != 0) {
            ERRLOG("OOPS avail_in != 0 ...");
            return UNKNOWN_ERR;
        }

        /* done when last data in file processed */
    } while (flush != Z_FINISH);
    if (ret != Z_STREAM_END) {
        ERRLOG("OOPS != Z_STREAM_END ...");
        return UNKNOWN_ERR;
    }

    (void)deflateEnd(&strm);
    return Z_OK;
}

static int const _inf(FILE *source, FILE *dest) {
    unsigned have;
    z_stream strm;
    unsigned char in[CHUNK];
    unsigned char out[CHUNK];

    /* allocate inflate state */
    strm.zalloc = Z_NULL;
    strm.zfree = Z_NULL;
    strm.opaque = Z_NULL;
    strm.avail_in = 0;
    strm.next_in = Z_NULL;
    int ret = inflateInit(&strm);
    if (ret != Z_OK) {
        return ret;
    }

    /* decompress until deflate stream ends or end of file */
    do {
        strm.avail_in = fread(in, 1, CHUNK, source);
        if (ferror(source)) {
            (void)inflateEnd(&strm);
            ERRLOG("OOPS inflateEnd ...");
            return Z_ERRNO;
        }
        if (strm.avail_in == 0)
        {
            ERRLOG("OOPS avail_in != 0 ...");
            return UNKNOWN_ERR;
        }
        strm.next_in = in;

        /* run inflate() on input until output buffer not full */
        do {
            strm.avail_out = CHUNK;
            strm.next_out = out;
            ret = inflate(&strm, Z_NO_FLUSH);
            if (ret == Z_STREAM_ERROR) {
                ERRLOG("OOPS inflate : %d", ret);
                return ret;
            }
            switch (ret) {
            case Z_NEED_DICT:
                ret = Z_DATA_ERROR;     /* and fall through */
            case Z_DATA_ERROR:
            case Z_MEM_ERROR:
                (void)inflateEnd(&strm);
                ERRLOG("OOPS : %d", ret);
                return ret;
            }
            have = CHUNK - strm.avail_out;
            if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
                (void)inflateEnd(&strm);
                ERRLOG("OOPS fwrite ...");
                return Z_ERRNO;
            }
        } while (strm.avail_out == 0);

        /* done when inflate() says it's done */
    } while (ret != Z_STREAM_END);

    /* clean up and return */
    (void)inflateEnd(&strm);
    return Z_STREAM_END ? Z_OK : Z_DATA_ERROR;
}

/* report a zlib or i/o error */
static const char* const zerr(int ret, FILE *in, FILE *out)
{
    switch (ret) {
    case Z_ERRNO:
        if (in && ferror(in))
        {
            return "error reading";
        }
        if (out && ferror(out))
        {
            return "error writing";
        }
        ERRLOG("Unknown zerr...");
    case Z_OK:
        return NULL;

    case Z_STREAM_ERROR:
        return "invalid compression level";

    case Z_DATA_ERROR:
        return "invalid/incomplete deflate data";

    case Z_MEM_ERROR:
        return "out of memory";

    case Z_VERSION_ERROR:
        return "zlib version mismatch!";

    default:
        return "general zlib error";
    }
}


/* Compress from file source to file dest until EOF on source.
   def() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_STREAM_ERROR if an invalid compression
   level is supplied, Z_VERSION_ERROR if the version of zlib.h and the
   version of the library linked do not match, or Z_ERRNO if there is
   an error reading or writing the files. */
const char* const def(const char* const src, const int level)
{
    FILE *source = NULL;
    FILE *dest = NULL;
    int ret = 0;

    if (src == NULL) {
        return NULL;
    }

    do {

        source = fopen(src, "r");
        if (source == NULL) {
            ERRLOG("Cannot open file '%s' for reading", src);
            ret = UNKNOWN_ERR;
            break;
        }

        char dst[TEMPSIZE];
        snprintf(dst, TEMPSIZE-1, "%s%s", src, ".gz");
        FILE *dest = fopen(dst, "w+");

        if (dest == NULL) {
            ERRLOG("Cannot open file '%s' for writing", dst);
            ret = UNKNOWN_ERR;
            break;
        }

        ret = _def(source, dest, level);
    } while (0);

    const char* const err = zerr(ret, source, dest);

    // clean up

    if (source) {
        fclose(source);
    }

    fflush(dest);

    if (dest) {
        fclose(dest);
    }

    return err;
}

/* Decompress from file source to file dest until stream ends or EOF.
   inf() returns Z_OK on success, Z_MEM_ERROR if memory could not be
   allocated for processing, Z_DATA_ERROR if the deflate data is
   invalid or incomplete, Z_VERSION_ERROR if the version of zlib.h and
   the version of the library linked do not match, or Z_ERRNO if there
   is an error reading or writing the files. */
const char* const inf(const char* const src)
{
    FILE *source = NULL;
    FILE *dest = NULL;
    int ret = 0;

    if (src == NULL) {
        return NULL;
    }

    do {
        FILE *source = fopen(src, "r");
        if (source == NULL) {
            ERRLOG("Cannot open file '%s' for reading", src);
            ret = UNKNOWN_ERR;
            break;
        }

        int len = strlen(src);
        char dst[TEMPSIZE];
        snprintf(dst, TEMPSIZE-1, "%s", src);
        if (! ( (dst[len-3] == '.') && (dst[len-2] == 'g') && (dst[len-1] == 'z') ) ) {
            ERRLOG("Expected filename ending in .gz");
            ret = UNKNOWN_ERR;
            break;
        }
        dst[len-3] = '\0';

        FILE *dest = fopen(dst, "w+");
        if (dest == NULL) {
            ERRLOG("Cannot open file '%s' for writing", dst);
            ret = UNKNOWN_ERR;
            break;
        }

        ret = _inf(source, dest);

    } while (0);

    const char* const err = zerr(ret, source, dest);

    // clean up

    if (source) {
        fclose(source);
    }

    fflush(dest);

    if (dest) {
        fclose(dest);
    }

    return err;
}

