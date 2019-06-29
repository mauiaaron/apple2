/*
 * Apple // emulator for *ix 
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2013-2017 Aaron Culliney
 *
 */

#include "common.h"

typedef enum a2gzip_t {
    A2GZT_ERR = -1,
    A2GZT_NOT_GZ = 0,
    A2GZT_MAYBE_GZ = 1,
} a2gzip_t;

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

static int _gzread_data(gzFile gzsource, uint8_t *buf, const unsigned int expected_bytescount) {
    int bytescount = 0;

    int maxtries = 10;
    do {
        int bytesread = gzread(gzsource, buf+bytescount, expected_bytescount-bytescount);
        if (bytesread <= 0) {
            if (--maxtries == 0) {
                LOG("OOPS, giving up on gzread() ...");
                break;
            }
            LOG("OOPS, gzread() ...");
            usleep(100);
            continue;
        }

        bytescount += bytesread;

        if (bytescount >= expected_bytescount) {
            bytescount = expected_bytescount;
            break; // DONE
        }
    } while (1);

    return bytescount;
}

static ssize_t _read_data(int fd_own, uint8_t *buf, const off_t expected_bytescount) {
    ssize_t bytescount = 0;

    int maxtries = 10;
    do {
        ssize_t bytesread = 0;
        off_t len0 = expected_bytescount-bytescount;
        unsigned int len = (unsigned int)len0;
        if (UNLIKELY(len0 > UINT_MAX || len0 < 0)) {
            assert(false);
        }

        TEMP_FAILURE_RETRY(bytesread = read(fd_own, buf+bytescount, len));
        if (bytesread <= 0) {
            if (--maxtries == 0) {
                LOG("OOPS, giving up on read() ... (%s)", strerror(errno));
                break;
            }
            LOG("OOPS, read() ... (%s)", strerror(errno));
            usleep(100);
            continue;
        }

        bytescount += bytesread;

        if (bytescount >= expected_bytescount) {
            bytescount = (int)expected_bytescount;
            break; // DONE
        }
    } while (1);

    return bytescount;
}


static ssize_t _write_data(int fd_own, uint8_t *buf, const unsigned int expected_bytescount) {
    ssize_t bytescount = 0;

    int maxtries = 10;
    do {
        ssize_t byteswritten = 0;
        TEMP_FAILURE_RETRY(byteswritten = write(fd_own, buf+bytescount, expected_bytescount-bytescount));

        if (byteswritten <= 0) {
            if (--maxtries == 0) {
                LOG("OOPS, giving up on write() ... (%s)", strerror(errno));
                break;
            }
            LOG("OOPS, write ... (%s)", strerror(errno));
            usleep(100);
            continue;
        }

        bytescount += byteswritten;

        if (bytescount >= expected_bytescount) {
            bytescount = expected_bytescount;
            break; // DONE
        }
    } while (1);

    return bytescount;
}

static a2gzip_t _check_gzip_magick(int fd_own, const unsigned int expected_bytescount) {

    a2gzip_t ret = A2GZT_ERR;

    do {
        uint8_t stkbuf[2];
        off_t bytescount = _read_data(fd_own, &stkbuf[0], sizeof(stkbuf));
        if (bytescount != sizeof(stkbuf)) {
            LOG("OOPS, could not read file magick for file descriptor");
            break;
        }

        bytescount = lseek(fd_own, 0L, SEEK_END);
        if (bytescount == -1) {
            LOG("OOPS, cannot seek to end of file descriptor! (%s)", strerror(errno));
            break;
        }

        if (stkbuf[0] == 0x1f && stkbuf[1] == 0x8b) {
            // heuristics : gzip magick matches, but could possibly be a disk image file that begins with those bytes ...
            ret = A2GZT_MAYBE_GZ;
            if (bytescount >= expected_bytescount) {
                // most gzipped disk images are *likely* to compress smaller, but we shouldn't assume ...
                LOG("OMG, found a large apparently gzipped disk image!");
            }
        } else {
            // did not see the gzip magick, so this is not a gzip file, but we should check file length ...
            if (bytescount >= expected_bytescount) {
                ret = A2GZT_NOT_GZ;
            } else {
                LOG("OOPS, did not find gzip magick, and file is %lld bytes, not expected size of %u", (long long)bytescount, expected_bytescount);
            }
        }
    } while (0);

    return ret;
}

/* Inflate/uncompress file descriptor to buffer.
 *
 * Return NULL on success, or error string (possibly from zlib) on failure.
 */
const char *zlib_inflate_to_buffer(int fd, const unsigned int expected_bytescount, uint8_t *buf) {
    gzFile gzsource = NULL;
    int fd_own = -1;

    assert(buf != NULL);
    assert(expected_bytescount > 0);

    ssize_t bytescount = 0;
    char *err = NULL;
    do {

        TEMP_FAILURE_RETRY(fd_own = dup(fd)); // balance gzclose() behavior
        if (fd_own == -1) {
            LOG("OOPS, could not dup() file descriptor %d", fd);
            break;
        }
        fd = -1;

        // check gzip magick ...
        a2gzip_t val = _check_gzip_magick(fd_own, expected_bytescount);
        if (val == A2GZT_ERR) {
            break;
        }

        if (lseek(fd_own, 0L, SEEK_SET) == -1) {
            LOG("OOPS, cannot seek to start of file descriptor! (%s)", strerror(errno));
            break;
        }

        if (val == A2GZT_NOT_GZ) {
            // definitively not gzipped and expected size, all good!
            bytescount = _read_data(fd_own, buf, expected_bytescount);
            break;
        }

        // attempt inflate ...

        gzsource = gzdopen(fd_own, "r");
        if (gzsource == NULL) {
            LOG("OOPS, cannot open file descriptor for reading");
            break;
        }

        bytescount = _gzread_data(gzsource, buf, expected_bytescount);
        if (bytescount != expected_bytescount) {
            // could not gzread(), maybe it's not actually a gzip stream? ...
            LOG("OOPS, did not gzread() expected_bytescount of %u ... apparently read %zd ... checking file length heuristic ...", expected_bytescount, bytescount);

            if (lseek(fd_own, 0L, SEEK_SET) == -1) {
                LOG("OOPS, cannot seek to start of file descriptor! (%s)", strerror(errno));
                break;
            }

            bytescount = _read_data(fd_own, buf, expected_bytescount);

            if (bytescount < expected_bytescount) {
                LOG("OOPS, apparently corrupt gzipped file...");
            } else {
                LOG("File appears to be non-gzipped, proceeding to load ...");
                bytescount = expected_bytescount;
            }

            break;
        }

    } while (0);

    if (bytescount != expected_bytescount) {
        LOG("OOPS did not read expected_bytescount of %u ... apparently read %zd", expected_bytescount, bytescount);
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
        // TEMP_FAILURE_RETRY(close(fd_own)); -- NOTE gzdopen() API with gzclose() above also closes the dup()'d fd
    } else if (fd_own > 0) {
        TEMP_FAILURE_RETRY(close(fd_own));
    }

    return err;
}

/* Inline inflate/uncompress buffer to file descriptor
 *
 * Return NULL on success, or error string (possibly from zlib) on failure.
 */
const char *zlib_inflate_inplace(int fd, const unsigned int expected_bytescount, bool *is_gzipped) {
    gzFile gzsource = NULL;
    int fd_own = -1;
    uint8_t *buf = NULL;

    *is_gzipped = false;

    assert(expected_bytescount > 2);

    off_t bytescount = 0;
    do {

        TEMP_FAILURE_RETRY(fd_own = dup(fd)); // balance gzclose()
        if (fd_own == -1) {
            LOG("OOPS, could not dup() file descriptor %d", fd);
            break;
        }
        fd = -1;

        // check gzip magick ...
        a2gzip_t val = _check_gzip_magick(fd_own, expected_bytescount);
        if (val == A2GZT_ERR) {
            break;
        } else if (val == A2GZT_NOT_GZ) {
            // definitively not gzipped and expected size, all good!
            bytescount = expected_bytescount;
            break;
        }

        // attempt inflate ...

        buf = MALLOC(expected_bytescount);
        if (buf == NULL) {
            LOG("OOPS, failed allocation of %d bytes", expected_bytescount);
            break;
        }

        if (lseek(fd_own, 0L, SEEK_SET) == -1) {
            LOG("OOPS, cannot seek to start of file descriptor! (%s)", strerror(errno));
            break;
        }

        gzsource = gzdopen(fd_own, "r");
        if (gzsource == NULL) {
            LOG("OOPS, cannot open file file descriptor for reading");
            break;
        }

        bytescount = _gzread_data(gzsource, buf, expected_bytescount);
        if (bytescount != expected_bytescount) {
            // could not gzread(), maybe it's not actually a gzip stream? ...
            LOG("OOPS, did not in-place gzread() expected_bytescount of %u ... apparently read %lld ... checking file length heuristic ...", expected_bytescount, (long long)bytescount);

            bytescount = lseek(fd_own, 0L, SEEK_END);
            if (bytescount == -1) {
                LOG("OOPS, cannot seek to end of file descriptor! (%s)", strerror(errno));
                break;
            }

            if (bytescount < expected_bytescount) {
                LOG("OOPS, apparently corrupt gzipped file...");
            } else {
                LOG("File appears to be non-gzipped, proceeding to load ...");
                bytescount = expected_bytescount;
            }

            break;
        }

        // successfully read gzipped file ...
        *is_gzipped = true;

        // inplace write file

        if (lseek(fd_own, 0L, SEEK_SET) == -1) {
            LOG("OOPS, cannot seek to start of file descriptor! (%s)", strerror(errno));
            break;
        }

        bytescount = _write_data(fd_own, buf, expected_bytescount);

        if (bytescount == expected_bytescount) {
            int ret = -1;
            TEMP_FAILURE_RETRY(ret = ftruncate(fd_own, expected_bytescount));
            if (ret == -1) {
                LOG("OOPS, ftruncate file descriptor failed");
                break;
            }
        }

    } while (0);

    char *err = NULL;
    if (bytescount != expected_bytescount) {
        LOG("OOPS, did not write expected_bytescount of %u ... apparently wrote %lld", expected_bytescount, (long long)bytescount);
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
        // TEMP_FAILURE_RETRY(close(fd_own)); -- NOTE gzdopen() API with gzclose() above also closes the dup()'d fd
    } else if (fd_own > 0) {
        TEMP_FAILURE_RETRY(close(fd_own));
    }

    if (buf) {
        FREE(buf);
    }

    return err;
}

/* Deflate/compress source buffer to destination buffer.
 *
 * Return NULL on success, or error string (possibly from zlib) on failure.
 */
const char *zlib_deflate_buffer(const uint8_t *src, const unsigned int src_bytescount, uint8_t *dst, OUTPARM off_t *dst_size) {
    char *gzPath = NULL;
    gzFile gzdest = NULL;
    int fd_own = -1;

    off_t expected_bytescount = src_bytescount;

    *dst_size = -1;

    off_t bytescount = 0;
    char *err = NULL;
    do {
        ASPRINTF(&gzPath, "%s/tmp.img", data_dir);
        assert(gzPath != NULL);

        TEMP_FAILURE_RETRY(fd_own = open(gzPath, O_RDWR|O_CREAT|O_TRUNC, S_IRUSR|S_IWUSR));
        if (fd_own == -1) {
            LOG("OOPS could not open temp image path : %s", gzPath);
            break;
        }

        gzdest = gzdopen(fd_own, "w");
        if (gzdest == NULL) {
            LOG("OOPS, cannot open file descriptor for writing");
            break;
        }

        do {
            off_t len0 = expected_bytescount-bytescount;
            unsigned int len = (unsigned int)len0;
            if (UNLIKELY(len0 > UINT_MAX || len0 < 0)) {
                assert(false);
            }
            int byteswritten = gzwrite(gzdest, src+bytescount, len);
            if (byteswritten <= 0) {
                LOG("OOPS, gzwrite() returned %d", byteswritten);
                break;
            }

            bytescount += byteswritten;
            if (bytescount >= expected_bytescount) {
                bytescount = expected_bytescount;
                break;
            }

            // do short writes happen? ...
        } while (1);

        if (bytescount != expected_bytescount) {
            break;
        }

        if (gzflush(gzdest, Z_FINISH) != Z_OK) {
            LOG("Error flushing compressed output : %s", err);
            break;
        }

        // now read compressed data into buffer ...

        {
            off_t compressed_size = lseek(fd_own, 0L, SEEK_CUR);
            if (compressed_size == -1) {
                LOG("OOPS, cannot seek to current position! (%s)", strerror(errno));
                break;
            }
            assert(/*compressed_size > 0 && */compressed_size < expected_bytescount);
            expected_bytescount = compressed_size;
        }

        if (lseek(fd_own, 0L, SEEK_SET) == -1) {
            LOG("OOPS, cannot seek to start of file descriptor! (%s)", strerror(errno));
            break;
        }

        bytescount = _read_data(fd_own, dst, expected_bytescount);
        if (bytescount != expected_bytescount) {
            break;
        }

        *dst_size = expected_bytescount;

    } while (0);

    if (bytescount != expected_bytescount) {
        LOG("OOPS, did not write/read expected number of bytes of %lld ... apparently wrote %lld", (long long)expected_bytescount, (long long)bytescount);
        if (gzdest) {
            err = (char *)_gzerr(gzdest);
        }
        if (!err) {
            err = ZERR_UNKNOWN;
        }
    }

    // clean up

    if (gzdest) {
        gzclose(gzdest);
        // TEMP_FAILURE_RETRY(close(fd_own)); -- NOTE gzdopen() API with gzclose() above also closes the open()'d fd
    } else if (fd_own > 0) {
        TEMP_FAILURE_RETRY(close(fd_own));
    }

    if (gzPath) {
        unlink(gzPath);
        FREE(gzPath);
    }

    return err;
}

