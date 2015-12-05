//
// Sigh ... adb shell changes LF to CRLF in binary streams ...
//
// http://stackoverflow.com/questions/13578416/read-binary-stdout-data-from-adb-shell
//
// Problem:
//  ( adb shell run-as com.example.someapp dd if=/data/data/com.example.someapp/some_binfile.bin 2\>/dev/null ) > local_binfile.bin
//
//  * Without piping through adb_sanitize you would get a corrupted binary
//
// Fix:
//  ( adb shell run-as com.example.someapp dd if=/data/data/com.example.someapp/some_binfile.bin 2\>/dev/null ) | adb_sanitize > local_binfile.bin
//
// Addenda:
//  * The only other way to pull stuff from /data/data/... appears to be to use the heavyweight 'adb backup', yuck
//

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdbool.h>
#include <unistd.h>
#include <sys/errno.h>

// cribbed from AOSP and modified with usleep() and to also ignore EAGAIN
#undef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(exp) ({ \
    typeof (exp) _rc; \
    do { \
        _rc = (exp); \
        if (_rc == -1 && (errno == EINTR || errno == EAGAIN) ) { \
            usleep(10); \
        } else { \
            break; \
        } \
    } while (1); \
    _rc; })

static int _convert_crlf_to_lf(void) {

    const char CR = 0x0d;
    const char LF = 0x0a;

    ssize_t inlen = 0;
    char inbuf[BUFSIZ];
    char outbuf[BUFSIZ];
    bool sawCR = false;
    char *errRd = NULL;
    char *errWrt = NULL;

    while ( (inlen = TEMP_FAILURE_RETRY(read(STDIN_FILENO, inbuf, BUFSIZ))) ) {
        if (inlen == -1) {
            errRd = "error reading from stdin";
            break;
        }

        // convert CRLF -> LF

        ssize_t outmax=0;
        for (ssize_t i=0; i<inlen; i++) {
            char c = inbuf[i];

            if (sawCR && (c != LF)) {
                outbuf[outmax++] = CR;
            }

            sawCR = false;
            if (c == CR) {
                sawCR = true;
            } else {
                outbuf[outmax++] = c;
            }
        }

        ssize_t outlen = 0;
        do {
            if (TEMP_FAILURE_RETRY(outlen = write(STDOUT_FILENO, outbuf, outmax)) == -1) {
                errWrt = "error writing to stdout";
                break;
            }
            outbuf += outlen;
            outmax -= outlen;
        } while (outmax > 0);
    }

    if (sawCR) {
        if (TEMP_FAILURE_RETRY(write(STDOUT_FILENO, &CR, 1)) == -1) {
            errWrt = "err writing to stdout";
        }
    }

    if (errRd) {
        fprintf(stderr, "%s : %s", errRd, strerror(errno));
        return 1;
    }
    if (errWrt) {
        fprintf(stderr, "%s : %s", errWrt, strerror(errno));
        return 2;
    }

    TEMP_FAILURE_RETRY(fsync(STDOUT_FILENO));
    return 0;
}

int main(int argc, char **argv, char **envp) {
#if UNIT_TEST
#error TODO FIXME ... should test edge cases CRLF split between buffered reads, and file/stream terminating 0D ([...0D][0A...] , [...0D])
    _run_unit_tests();
#else
    return _convert_crlf_to_lf();
#endif
}
