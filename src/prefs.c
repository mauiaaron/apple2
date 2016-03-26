/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 1994 Alexander Jean-Claude Bottema
 * Copyright 1995 Stephen Lee
 * Copyright 1997, 1998 Aaron Culliney
 * Copyright 1998, 1999, 2000 Michael Deutschmann
 * Copyright 2013-2015 Aaron Culliney
 *
 */

#include "prefs.h"
#include "json_parse_private.h"

typedef struct prefs_listener_s {
    prefs_change_callback_f prefsChanged;
    struct prefs_listener_s *nextListener;
} prefs_listener_s;

typedef struct prefs_domain_s {
    char *domain;
    struct prefs_listener_s *listeners;
    struct prefs_domain_s *nextDomain;
} prefs_domain_s;

static JSON_ref jsonPrefs = NULL;
static prefs_domain_s *domains = NULL;
static char *prefsFile = NULL;

static pthread_mutex_t prefsLock = PTHREAD_MUTEX_INITIALIZER;

// ----------------------------------------------------------------------------

void prefs_load(void) {
    FREE(prefsFile);

    const char *apple2JSON = getenv("APPLE2IX_JSON");
    if (apple2JSON) {
        ASPRINTF(&prefsFile, "%s", apple2JSON);
    }

    if (!prefsFile) {
        ASPRINTF(&prefsFile, "%s/.apple2.json", getenv("HOME"));
    }
    assert(prefsFile);

    json_destroy(&jsonPrefs);
    int tokCount = json_createFromFile(prefsFile, &jsonPrefs);
    if (tokCount < 0) {
        tokCount = json_createFromString("{}", &jsonPrefs);
        assert(tokCount > 0);
    }

    prefs_sync(NULL);
}

void prefs_loadString(const char *jsonString) {
    json_destroy(&jsonPrefs);
    int tokCount = json_createFromString(jsonString, &jsonPrefs);
    if (tokCount < 0) {
        tokCount = json_createFromString("{}", &jsonPrefs);
        assert(tokCount > 0);
    }

    prefs_sync(NULL);
}

bool prefs_save(void) {

    int fd = -1;
    bool success = false;
    do {
        if (!prefsFile) {
            ERRLOG("Not saving preferences, no file loaded...");
            break;
        }

        if (!jsonPrefs) {
            ERRLOG("Not saving preferences, none loaded...");
            break;
        }

        if (((JSON_s *)jsonPrefs)->numTokens <= 0) {
            ERRLOG("Not saving preferences, no preferences loaded...");
            break;
        }

        assert(((JSON_s *)jsonPrefs)->jsonString && "string should be valid");

#define PREFS_ERRPRINT() \
        ERRLOG( \
                "Cannot open the .apple2.json preferences file for writing.\n" \
                "Make sure it has R/W permission in your home directory.")

#define ERROR_SUBMENU_H 8
#define ERROR_SUBMENU_W 40
#if defined(INTERFACE_CLASSIC) && !TESTING
        int ch = -1;
        char submenu[ERROR_SUBMENU_H][ERROR_SUBMENU_W+1] =
            //1.  5.  10.  15.  20.  25.  30.  35.  40.
        { "||||||||||||||||||||||||||||||||||||||||",
            "|                                      |",
            "|                                      |",
            "| OOPS, could not open or write to the |",
            "| .apple2.json preferences file        |",
            "|                                      |",
            "|                                      |",
            "||||||||||||||||||||||||||||||||||||||||" };
#endif

        TEMP_FAILURE_RETRY(fd = open(prefsFile, O_WRONLY|O_CREAT, S_IRUSR|S_IWUSR));
        if (fd == -1) {
            PREFS_ERRPRINT();
#if defined(INTERFACE_CLASSIC) && !TESTING
            c_interface_print_submenu_centered(submenu[0], ERROR_SUBMENU_W, ERROR_SUBMENU_H);
            while ((ch = c_mygetch(1)) == -1) {
                // ...
            }
#endif
            break;
        }

        success = json_serialize(jsonPrefs, fd, /*pretty:*/true);
    } while (0);

    if (fd != -1) {
        TEMP_FAILURE_RETRY(fsync(fd));
        TEMP_FAILURE_RETRY(close(fd));
    }

    return success;
}

bool prefs_copyStringValue(const char *domain, const char *key, INOUT char **val) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount < 0) {
            break;
        }

        ret = json_mapCopyStringValue(jsonRef, key, val);
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

bool prefs_parseLongValue(const char *domain, const char *key, INOUT long *val, const long base) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount < 0) {
            break;
        }

        ret = json_mapParseLongValue(jsonRef, key, val, base);
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

bool prefs_parseBoolValue(const char *domain, const char *key, INOUT bool *val) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount < 0) {
            break;
        }

        ret = json_mapParseBoolValue(jsonRef, key, val);
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

bool prefs_parseFloatValue(const char *domain, const char *key, INOUT float *val) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount < 0) {
            break;
        }

        ret = json_mapParseFloatValue(jsonRef, key, val);
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

bool prefs_setStringValue(const char *domain, const char * _NONNULL key, const char * _NONNULL val) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount <= 0) {
            errCount = json_createFromString("{}", &jsonRef);
            assert(errCount > 0);
            assert(jsonRef);
        }

        ret = json_mapSetStringValue(jsonRef, key, val);
        if (!ret) {
            break;
        }

        ret = json_mapSetJSONValue(jsonPrefs, domain, jsonRef);
        if (!ret) {
            break;
        }
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

bool prefs_setLongValue(const char * _NONNULL domain, const char * _NONNULL key, long val) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount <= 0) {
            errCount = json_createFromString("{}", &jsonRef);
            assert(errCount > 0);
            assert(jsonRef);
        }

        ret = json_mapSetLongValue(jsonRef, key, val);
        if (!ret) {
            break;
        }

        ret = json_mapSetJSONValue(jsonPrefs, domain, jsonRef);
        if (!ret) {
            break;
        }
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

bool prefs_setBoolValue(const char * _NONNULL domain, const char * _NONNULL key, bool val) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount <= 0) {
            errCount = json_createFromString("{}", &jsonRef);
            assert(errCount > 0);
            assert(jsonRef);
        }

        ret = json_mapSetBoolValue(jsonRef, key, val);
        if (!ret) {
            break;
        }

        ret = json_mapSetJSONValue(jsonPrefs, domain, jsonRef);
        if (!ret) {
            break;
        }
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

bool prefs_setFloatValue(const char * _NONNULL domain, const char * _NONNULL key, float val) {
    pthread_mutex_lock(&prefsLock);

    bool ret = false;
    JSON_ref jsonRef = NULL;
    do {
        int errCount = json_mapCopyJSON(jsonPrefs, domain, &jsonRef);
        if (errCount <= 0) {
            errCount = json_createFromString("{}", &jsonRef);
            assert(errCount > 0);
            assert(jsonRef);
        }

        ret = json_mapSetFloatValue(jsonRef, key, val);
        if (!ret) {
            break;
        }

        ret = json_mapSetJSONValue(jsonPrefs, domain, jsonRef);
        if (!ret) {
            break;
        }
    } while (0);

    pthread_mutex_unlock(&prefsLock);
    json_destroy(&jsonRef);

    return ret;
}

void prefs_registerListener(const char *domain, prefs_change_callback_f callback) {
    pthread_mutex_lock(&prefsLock);

    assert(domain && "listener needs to specify non-NULL domain");

    prefs_domain_s *dom = domains;
    while (dom) {
        if (strcmp(domain, dom->domain) == 0) {
            break;
        }
        dom = dom->nextDomain;
    }

    if (!dom) {
        dom = MALLOC(sizeof(*dom));
        dom->domain = STRDUP(domain);
        dom->nextDomain = domains;
        dom->listeners = NULL;
        domains = dom;
    }

    prefs_listener_s *newL = MALLOC(sizeof(*newL));
    prefs_listener_s *oldL = dom->listeners;
    dom->listeners = newL;
    newL->nextListener = oldL;
    newL->prefsChanged = callback;

    pthread_mutex_unlock(&prefsLock);
}

void prefs_sync(const char *domain) {
    static int syncCount = 0;
    pthread_mutex_lock(&prefsLock);
    ++syncCount;
    if (syncCount > 1) {
        pthread_mutex_unlock(&prefsLock);
        return;
    }
    pthread_mutex_unlock(&prefsLock);

    prefs_domain_s *dom = domains;
    do {
        while (dom) {
            if (domain && (strcmp(domain, dom->domain) != 0)) {
                continue;
            }

            prefs_listener_s *listener = dom->listeners;
            while (listener) {
                listener->prefsChanged(dom->domain);
                listener = listener->nextListener;
            }

            if (domain) {
                break;
            }

            dom = dom->nextDomain;
        }

        pthread_mutex_lock(&prefsLock);
        --syncCount;
        if (syncCount == 0) {
            pthread_mutex_unlock(&prefsLock);
            break;
        }
        pthread_mutex_unlock(&prefsLock);

    } while (1);
}

void prefs_shutdown(bool emulatorShuttingDown) {
    if (!emulatorShuttingDown) {
        return;
    }

    FREE(prefsFile);

    pthread_mutex_lock(&prefsLock);

    prefs_domain_s *dom = domains;
    domains = NULL;

    while (dom) {
        prefs_listener_s *listener = dom->listeners;
        while (listener) {
            prefs_listener_s *dead = listener;
            listener = listener->nextListener;
            FREE(dead);
        }

        prefs_domain_s *dead = dom;
        dom = dom->nextDomain;

        FREE(dead->domain);
        FREE(dead);
    }

    pthread_mutex_unlock(&prefsLock);
}

