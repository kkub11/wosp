/* SPDX-License-Identifier: GPL-3.0-or-later */
/* Copyright (C) 2025 Andrew Trettel */
#ifndef CACHE_H
#define CACHE_H

#include <stdbool.h>
#include "words.h"

bool try_load_cache(const char *input_path, char *filename, Word **list_out);
void save_cache(const char *input_path, Word *list);

#endif /* CACHE_H */
