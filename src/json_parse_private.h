/*
 * Apple // emulator for *ix
 *
 * This software package is subject to the GNU General Public License
 * version 3 or later (your choice) as published by the Free Software
 * Foundation.
 *
 * Copyright 2015-2016 Aaron Culliney
 *
 */

typedef struct JSON_s {
    size_t jsonLen;
    char *jsonString;
    int numTokens;
    jsmntok_t *jsonTokens;
} JSON_s;

