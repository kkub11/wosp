/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2025 Andrew Trettel */
#define _GNU_SOURCE
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

#include "cache.h"
#include "misc.h"
#include "words.h"

static const char CACHE_MAGIC[4] = {'W', 'O', 'S', 'P'};
static const uint32_t CACHE_VERSION = 1;

static uint64_t
fnv1a_64(const unsigned char *data, size_t n)
{
    uint64_t h = UINT64_C(14695981039346656037);
    size_t i;
    for (i = 0; i < n; i++)
    {
        h ^= (uint64_t)data[i];
        h *= UINT64_C(1099511628211);
    }
    return h;
}

static uint64_t
hash_file(const char *path)
{
    FILE *f = fopen(path, "rb");
    if (f == NULL) return 0;
    uint64_t h = UINT64_C(14695981039346656037);
    int c;
    while ((c = fgetc(f)) != EOF)
    {
        h ^= (uint64_t)(unsigned char)c;
        h *= UINT64_C(1099511628211);
    }
    fclose(f);
    return h;
}

static void
ensure_cache_dir(const char *dir)
{
    char tmp[PATH_MAX];
    size_t len = strlen(dir);
    size_t i;
    if (len == 0 || len >= PATH_MAX) return;
    memcpy(tmp, dir, len + 1);
    for (i = 1; i <= len; i++)
    {
        if (tmp[i] == '/' || tmp[i] == '\0')
        {
            char saved = tmp[i];
            tmp[i] = '\0';
            (void)mkdir(tmp, 0755);
            tmp[i] = saved;
        }
    }
}

static void
get_cache_dir(char *buf, size_t buflen)
{
    const char *xdg = getenv("XDG_DATA_HOME");
    if (xdg != NULL && xdg[0] != '\0')
    {
        snprintf(buf, buflen, "%s/wosp", xdg);
        return;
    }
    const char *home = getenv("HOME");
    if (home == NULL || home[0] == '\0')
    {
        buf[0] = '\0';
        return;
    }
    snprintf(buf, buflen, "%s/.local/share/wosp", home);
}

static void
build_cache_path(const char *input_path, char *buf, size_t buflen)
{
    char real_path[PATH_MAX];
    char dir[PATH_MAX];

    if (realpath(input_path, real_path) == NULL)
    {
        buf[0] = '\0';
        return;
    }

    uint64_t h = fnv1a_64((const unsigned char *)real_path, strlen(real_path));
    get_cache_dir(dir, sizeof(dir));
    if (dir[0] == '\0')
    {
        buf[0] = '\0';
        return;
    }

    {
        size_t dir_len = strlen(dir);
        /* "/%016llx.wospidx" = 1+16+8+'\0' = 26 bytes */
        if (dir_len + 26 > buflen) { buf[0] = '\0'; return; }
        memcpy(buf, dir, dir_len);
        snprintf(buf + dir_len, buflen - dir_len, "/%016llx.wospidx",
                 (unsigned long long)h);
    }
}

bool
try_load_cache(const char *input_path, char *filename, Word **list_out)
{
    struct stat st;
    char magic[4];
    uint32_t version;
    int64_t cached_mtime;
    uint64_t cached_size;
    uint64_t cached_hash;
    uint32_t word_count;
    uint32_t i;
    char cpath[PATH_MAX];

    if (stat(input_path, &st) != 0) return false;

    build_cache_path(input_path, cpath, sizeof(cpath));
    if (cpath[0] == '\0') return false;

    FILE *f = fopen(cpath, "rb");
    if (f == NULL) return false;

    if (fread(magic, 1, 4, f) != 4 ||
        fread(&version, sizeof(uint32_t), 1, f) != 1 ||
        fread(&cached_mtime, sizeof(int64_t), 1, f) != 1 ||
        fread(&cached_size, sizeof(uint64_t), 1, f) != 1 ||
        fread(&cached_hash, sizeof(uint64_t), 1, f) != 1 ||
        fread(&word_count, sizeof(uint32_t), 1, f) != 1)
    {
        fclose(f);
        return false;
    }

    if (memcmp(magic, CACHE_MAGIC, 4) != 0 || version != CACHE_VERSION)
    {
        fclose(f);
        return false;
    }

    if (cached_mtime != (int64_t)st.st_mtime || cached_size != (uint64_t)st.st_size)
    {
        uint64_t actual_hash = hash_file(input_path);
        if (actual_hash != cached_hash)
        {
            fclose(f);
            return false;
        }
    }

    for (i = 0; i < word_count; i++)
    {
        uint32_t original_len;
        uint64_t line, column, position, page;
        char *data;

        if (fread(&original_len, sizeof(uint32_t), 1, f) != 1)
        {
            fclose(f);
            free_words(*list_out);
            *list_out = NULL;
            return false;
        }

        data = (char *)allocmem(original_len + 1, sizeof(char));

        if (fread(data, 1, original_len + 1, f) != original_len + 1)
        {
            free(data);
            fclose(f);
            free_words(*list_out);
            *list_out = NULL;
            return false;
        }

        if (fread(&line, sizeof(uint64_t), 1, f) != 1 ||
            fread(&column, sizeof(uint64_t), 1, f) != 1 ||
            fread(&position, sizeof(uint64_t), 1, f) != 1 ||
            fread(&page, sizeof(uint64_t), 1, f) != 1)
        {
            free(data);
            fclose(f);
            free_words(*list_out);
            *list_out = NULL;
            return false;
        }

        append_word(list_out, data, filename,
                    (unsigned long)line, (unsigned long)column,
                    (unsigned long)position, (unsigned long)page);
    }

    fclose(f);
    *list_out = list_first_word(*list_out);
    return true;
}

void
save_cache(const char *input_path, Word *list)
{
    struct stat st;
    char dir[PATH_MAX];
    char cpath[PATH_MAX];
    char tmp_path[PATH_MAX];
    uint32_t word_count;
    int64_t mtime_sec;
    uint64_t file_size;
    uint64_t content_hash;
    Word *w;
    bool ok;

    if (stat(input_path, &st) != 0) return;
    mtime_sec    = (int64_t)st.st_mtime;
    file_size    = (uint64_t)st.st_size;
    content_hash = hash_file(input_path);

    get_cache_dir(dir, sizeof(dir));
    if (dir[0] == '\0') return;
    ensure_cache_dir(dir);

    build_cache_path(input_path, cpath, sizeof(cpath));
    if (cpath[0] == '\0') return;

    if (snprintf(tmp_path, sizeof(tmp_path), "%s.tmp", cpath) >= (int)sizeof(tmp_path)) return;

    word_count = 0;
    w = list;
    while (w != NULL)
    {
        word_count++;
        w = next_word(w);
    }

    FILE *f = fopen(tmp_path, "wb");
    if (f == NULL)
    {
        fprintf(stderr, "wosp: warning: cannot write cache %s\n", tmp_path);
        return;
    }

    ok = true;
    ok = ok && (fwrite(CACHE_MAGIC, 1, 4, f) == 4);
    ok = ok && (fwrite(&CACHE_VERSION, sizeof(uint32_t), 1, f) == 1);
    ok = ok && (fwrite(&mtime_sec, sizeof(int64_t), 1, f) == 1);
    ok = ok && (fwrite(&file_size, sizeof(uint64_t), 1, f) == 1);
    ok = ok && (fwrite(&content_hash, sizeof(uint64_t), 1, f) == 1);
    ok = ok && (fwrite(&word_count, sizeof(uint32_t), 1, f) == 1);

    w = list;
    while (w != NULL && ok)
    {
        const char *orig = original_word(w);
        uint32_t orig_len = (uint32_t)strlen(orig);
        uint64_t line     = (uint64_t)line_word(w);
        uint64_t column   = (uint64_t)column_word(w);
        uint64_t position = (uint64_t)position_word(w);
        uint64_t page     = (uint64_t)page_word(w);

        ok = ok && (fwrite(&orig_len, sizeof(uint32_t), 1, f) == 1);
        ok = ok && (fwrite(orig, 1, orig_len + 1, f) == orig_len + 1);
        ok = ok && (fwrite(&line, sizeof(uint64_t), 1, f) == 1);
        ok = ok && (fwrite(&column, sizeof(uint64_t), 1, f) == 1);
        ok = ok && (fwrite(&position, sizeof(uint64_t), 1, f) == 1);
        ok = ok && (fwrite(&page, sizeof(uint64_t), 1, f) == 1);

        w = next_word(w);
    }

    fclose(f);

    if (ok)
    {
        if (rename(tmp_path, cpath) != 0)
        {
            fprintf(stderr, "wosp: warning: cannot finalize cache %s\n", cpath);
            (void)unlink(tmp_path);
        }
    }
    else
    {
        (void)unlink(tmp_path);
        fprintf(stderr, "wosp: warning: I/O error writing cache %s\n", tmp_path);
    }
}
