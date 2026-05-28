// SPDX-License-Identifier: GPL-3.0-or-later
/* Copyright (C) 2025 Andrew Trettel */
#include <assert.h>
#include <ctype.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "misc.h"
#include "words.h"

bool
is_truncation_character(char c)
{
    if (c == '$' || c == '#')
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool
is_ending_punctuation(char c)
{
    if (c == '.' || c == '?' || c == '!')
    {
        return true;
    }
    else
    {
        return false;
    }
}

bool
is_clause_punctuation(char c)
{
    if (c == ',' || c == ';' || c == ':' || is_ending_punctuation(c))
    {
        return true;
    }
    else
    {
        return false;
    }
}

char *
reduce_word(char *original, WordOrigin origin)
{
    size_t len = 0, j = 0;
    for (size_t i = 0; i < strlen(original); i++)
    {
        if ((ispunct(original[i]) == false) || ((original[i] == wildcard_character) && (origin == WO_QUERY)))
        {
            len++;
        }
    }
    len++;
    char *reduced = (char *) allocmem(len, sizeof(char));
    for (size_t i = 0; i < strlen(original); i++)
    {
        if ((ispunct(original[i]) == false) || ((original[i] == wildcard_character) && (origin == WO_QUERY)))
        {
            reduced[j] = original[i];
            j++;
        }
    }
    reduced[len-1] = '\0';
    return reduced;
}

void
append_word(Word **list, char *data, char *filename, unsigned long line,
            unsigned long column, unsigned long position, unsigned long page)
{
    Word *current = (Word *) allocmem(1, sizeof(Word));
    current->original = data;
    current->reduced = reduce_word(data, WO_SOURCE);
    current->filename = filename;
    current->line = line;
    current->column = column;
    current->position = position;
    current->page = page;
    current->field = full_text_field;
    current->next = NULL;
    current->prev = *list;
    current->clause_start    = NULL;
    current->clause_end      = NULL;
    current->line_start      = NULL;
    current->line_end        = NULL;
    current->sentence_start  = NULL;
    current->sentence_end    = NULL;
    current->paragraph_start = NULL;
    current->paragraph_end   = NULL;
    current->page_start      = NULL;
    current->page_end        = NULL;
    current->document        = NULL;
    if ((*list) != NULL)
    {
        (*list)->next = current;
    }
    *list = current;
}

char *
original_word(Word *word)
{
    return word->original;
}

char *
reduced_word(Word *word)
{
    return word->reduced;
}

char *
filename_word(Word *word)
{
    return word->filename;
}

unsigned long
line_word(Word *word)
{
    return word->line;
}

unsigned long
column_word(Word *word)
{
    return word->column;
}

unsigned long
position_word(Word *word)
{
    return word->position;
}

unsigned long
page_word(Word *word)
{
    return word->page;
}

unsigned long
field_word(Word *word)
{
    if (word == NULL)
    {
        return end_field;
    }
    else
    {
        return word->field;
    }
}

bool
field_has_next_word(Word *word)
{
    WordIterator iterator = init_word_iterator(word, next_word, true);
    if (iterator_has_next_word(iterator) == false)
    {
        return false;
    }
    else
    {
        iterator_next_word(&iterator);
        return iterator_has_next_word(iterator);
    }
}

bool
clause_ending_word(Word *word)
{
    if (word == NULL)
    {
        return true;
    }
    else
    {
        if (sentence_ending_word(word) == true)
        {
            return true;
        }
        else
        {
            char *data = original_word(word);
            size_t len = strlen(data);
            bool curr_cond = false;
            if (len == 1)
            {
                curr_cond = is_clause_punctuation(data[len-1]);
            }
            else
            {
                curr_cond = (is_clause_punctuation(data[len-1]) ||
                    (
                        is_clause_punctuation(data[len-2])
                        &&
                        (
                            (data[len-1] == '"')
                            ||
                            (data[len-1] == '\'')
                        )
                    )
                );
            }
            return curr_cond;
        }
    }
}

bool
sentence_ending_word(Word *word)
{
    if (word == NULL)
    {
        return true;
    }
    else
    {
        char *data = original_word(word);
        size_t len = strlen(data);
        bool curr_cond = false;
        if (len == 1)
        {
            curr_cond = is_ending_punctuation(data[len-1]);
        }
        else
        {
            curr_cond = (is_ending_punctuation(data[len-1]) ||
                (
                    is_ending_punctuation(data[len-2])
                    &&
                    (
                        (data[len-1] == '"')
                        ||
                        (data[len-1] == '\'')
                    )
                )
            );
        }
        if (field_has_next_word(word) == false)
        {
            return curr_cond;
        }
        else
        {
            char *next_data = original_word(next_word(word));
            size_t next_len = strlen(next_data);
            bool next_cond = false;
            if (next_len == 1)
            {
                next_cond = (isupper(next_data[0]) || isdigit(next_data[0]));
            }
            else
            {
                next_cond = (
                    (isupper(next_data[0]) || isdigit(next_data[0]))
                    ||
                    (
                        (isupper(next_data[1]) || isdigit(next_data[1]))
                        &&
                        (
                            (next_data[0] == '"')
                            ||
                            (next_data[0] == '\'')
                        )
                    )
                );
            }

            if ((curr_cond == true) && (next_cond == true))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

bool
paragraph_ending_word(Word *word)
{
    if (word == NULL)
    {
        return true;
    }
    else
    {
        bool sentence_cond = sentence_ending_word(word);
        if (field_has_next_word(word) == false)
        {
            return sentence_cond;
        }
        else
        {
            if ((sentence_cond == true) && (line_word(word) != line_word(next_word(word))))
            {
                return true;
            }
            else
            {
                return false;
            }
        }
    }
}

Word *
next_boolean_element(Word *word, bool element_ending_word(Word *))
{
    if (word == NULL)
    {
        return NULL;
    }
    else
    {
        Word *current = word;
        WordIterator iterator = init_word_iterator(word, next_word, true);
        while (iterator_has_next_word(iterator) == true)
        {
            current = iterator_next_word(&iterator);
            if ((element_ending_word(current) == true) && (iterator_has_next_word(iterator) == true))
            {
                return iterator_next_word(&iterator);
            }
        }
        return current;
    }
}

Word *
prev_boolean_element(Word *word, bool element_ending_word(Word *))
{
    if (word == NULL)
    {
        return NULL;
    }
    else
    {
        Word *current = word;
        WordIterator iterator = init_word_iterator(word, prev_word, true);
        if ((element_ending_word(current) == true) && (iterator_has_next_word(iterator) == true))
        {
            current = iterator_next_word(&iterator);
        }
        while (iterator_has_next_word(iterator) == true)
        {
            current = iterator_next_word(&iterator);
            if (element_ending_word(current) == true)
            {
                return next_word(current);
            }
        }
        return current;
    }
}

Word *
next_numbered_element(Word *word, unsigned long element_word(Word *))
{
    if (word == NULL)
    {
        return NULL;
    }
    else
    {
        unsigned long element = element_word(word);
        Word *current = word;
        WordIterator iterator = init_word_iterator(word, next_word, true);
        while (iterator_has_next_word(iterator) == true)
        {
            current = iterator_next_word(&iterator);
            if (element_word(current) != element)
            {
                return current;
            }
        }
        return current;
    }
}

Word *
prev_numbered_element(Word *word, unsigned long element_word(Word *))
{
    if (word == NULL)
    {
        return NULL;
    }
    else
    {
        unsigned long element = element_word(word);
        Word *current = word;
        WordIterator iterator = init_word_iterator(word, prev_word, true);
        while (iterator_has_next_word(iterator) == true)
        {
            current = iterator_next_word(&iterator);
            if (element_word(current) != element)
            {
                return next_word(current);
            }
        }
        return current;
    }
}

Word *
next_word(Word *word)
{
    if (word == NULL)
    {
        return NULL;
    }
    else
    {
        return word->next;
    }
}

Word *
prev_word(Word *word)
{
    if (word == NULL)
    {
        return NULL;
    }
    else
    {
        return word->prev;
    }
}

/* These use the boundary pointers populated by index_word_boundaries().
 * Semantics matching the original next_boolean_element / prev_boolean_element:
 *   next_X(w): first word of the NEXT element (or last word of list if none).
 *   prev_X(w): first word of the CURRENT element if w is not already there;
 *              otherwise first word of the previous element (or w itself if
 *              there is no previous one).
 * The asymmetry — next steps forward across the boundary, prev only steps to
 * the current element's start — is what advance_word(..., n) relies on to
 * traverse n elements correctly. */
static inline Word *
prev_element_start(Word *w, Word *current_start)
{
    if (w != current_start) return current_start;
    Word *p = prev_word(current_start);
    if (p == NULL) return current_start;
    /* p is the last word of the previous element; its *_start is what we want. */
    return p;  /* caller substitutes the correct *_start field */
}

Word *
next_clause(Word *word)
{
    if (word == NULL) return NULL;
    Word *n = next_word(word->clause_end);
    return (n == NULL) ? word->clause_end : n;
}

Word *
prev_clause(Word *word)
{
    if (word == NULL) return NULL;
    Word *p = prev_element_start(word, word->clause_start);
    return (p == word->clause_start) ? p : p->clause_start;
}

Word *
next_line(Word *word)
{
    if (word == NULL) return NULL;
    Word *n = next_word(word->line_end);
    return (n == NULL) ? word->line_end : n;
}

Word *
prev_line(Word *word)
{
    if (word == NULL) return NULL;
    Word *p = prev_element_start(word, word->line_start);
    return (p == word->line_start) ? p : p->line_start;
}

Word *
next_sentence(Word *word)
{
    if (word == NULL) return NULL;
    Word *n = next_word(word->sentence_end);
    return (n == NULL) ? word->sentence_end : n;
}

Word *
prev_sentence(Word *word)
{
    if (word == NULL) return NULL;
    Word *p = prev_element_start(word, word->sentence_start);
    return (p == word->sentence_start) ? p : p->sentence_start;
}

Word *
next_paragraph(Word *word)
{
    if (word == NULL) return NULL;
    Word *n = next_word(word->paragraph_end);
    return (n == NULL) ? word->paragraph_end : n;
}

Word *
prev_paragraph(Word *word)
{
    if (word == NULL) return NULL;
    Word *p = prev_element_start(word, word->paragraph_start);
    return (p == word->paragraph_start) ? p : p->paragraph_start;
}

Word *
next_page(Word *word)
{
    if (word == NULL) return NULL;
    Word *n = next_word(word->page_end);
    return (n == NULL) ? word->page_end : n;
}

Word *
prev_page(Word *word)
{
    if (word == NULL) return NULL;
    Word *p = prev_element_start(word, word->page_start);
    return (p == word->page_start) ? p : p->page_start;
}

static Word *
extreme_word(Word *word, Word *direction_word(Word *), bool limited_to_field)
{
    if (word == NULL)
    {
        return NULL;
    }
    else
    {
        Word *current = word;
        WordIterator iterator = init_word_iterator(word, direction_word, limited_to_field);
        while (iterator_has_next_word(iterator) == true)
        {
            current = iterator_next_word(&iterator);
        }
        return current;
    }
}

Word *
list_first_word(Word *word)
{
    return extreme_word(word, prev_word, false);
}

Word *
list_last_word(Word *word)
{
    return extreme_word(word, next_word, false);
}

Word *
field_first_word(Word *word)
{
    return extreme_word(word, prev_word, true);
}

Word *
field_last_word(Word *word)
{
    return extreme_word(word, next_word, true);
}

Word *
document_word(Word *word)
{
    if (word == NULL) return NULL;
    /* document pointer is populated by index_word_boundaries(); fall back to
     * the O(N) walk only when called before indexing (e.g. mid-parse). */
    return (word->document != NULL) ? word->document : list_first_word(word);
}

/* This is a wrapper function to handle the ends of the input word list safely.
 * The primitive operations can return NULL values.  This is necessary to check
 * for the ends of the list.  This function cannot return NULL values. */
Word *
advance_word(Word *word, LanguageElement element, int n)
{
    Word *current = word;
    if (n != 0)
    {
        Word *(*advance)(Word *) = NULL;
        if      (element == LE_WORD      && n > 0) {advance = next_word;}
        else if (element == LE_WORD      && n < 0) {advance = prev_word;}
        else if (element == LE_CLAUSE    && n > 0) {advance = next_clause;}
        else if (element == LE_CLAUSE    && n < 0) {advance = prev_clause;}
        else if (element == LE_LINE      && n > 0) {advance = next_line;}
        else if (element == LE_LINE      && n < 0) {advance = prev_line;}
        else if (element == LE_SENTENCE  && n > 0) {advance = next_sentence;}
        else if (element == LE_SENTENCE  && n < 0) {advance = prev_sentence;}
        else if (element == LE_PARAGRAPH && n > 0) {advance = next_paragraph;}
        else if (element == LE_PARAGRAPH && n < 0) {advance = prev_paragraph;}
        else if (element == LE_PAGE      && n > 0) {advance = next_page;}
        else if (element == LE_PAGE      && n < 0) {advance = prev_page;}
        assert(advance != NULL);
        size_t m = abs(n);
        WordIterator iterator = init_word_iterator(current, advance, true);
        size_t i = 0; /* Akin to an array index.  0 is word. */
        while ((i <= m) && (iterator_has_next_word(iterator) == true))
        {
            current = iterator_next_word(&iterator);
            i++;
        }
    }
    return current;
}

/* One-time O(N) pass that populates clause/line/sentence/paragraph/page
 * start/end pointers on every Word.  Must be called after the list is fully
 * built (whether from fresh parse or cache load) and before any query uses
 * prev_X / next_X / advance_word with element != LE_WORD.
 *
 * Note: currently all source words live in full_text_field, so a single
 * field per file.  If multi-field support is added, restart the *_s
 * trackers when field_word(w) changes. */
void
index_word_boundaries(Word *list)
{
    if (list == NULL) return;

    Word *clause_s = NULL, *line_s = NULL, *sentence_s = NULL,
         *paragraph_s = NULL, *page_s = NULL;
    bool prev_clause_end    = true;
    bool prev_sentence_end  = true;
    bool prev_paragraph_end = true;
    unsigned long prev_line_no = 0;
    unsigned long prev_page_no = 0;

    Word *head = list_first_word(list);
    Word *w = head;
    while (w != NULL)
    {
        if (prev_clause_end)                   clause_s    = w;
        if (prev_sentence_end)                 sentence_s  = w;
        if (prev_paragraph_end)                paragraph_s = w;
        if (line_s == NULL || line_word(w) != prev_line_no) line_s = w;
        if (page_s == NULL || page_word(w) != prev_page_no) page_s = w;

        w->clause_start    = clause_s;
        w->sentence_start  = sentence_s;
        w->paragraph_start = paragraph_s;
        w->line_start      = line_s;
        w->page_start      = page_s;
        w->document        = head;

        prev_clause_end    = clause_ending_word(w);
        prev_sentence_end  = sentence_ending_word(w);
        prev_paragraph_end = paragraph_ending_word(w);
        prev_line_no       = line_word(w);
        prev_page_no       = page_word(w);
        w = next_word(w);
    }

    /* Second pass: walk backward to fill *_end pointers. */
    w = list_last_word(list);
    Word *clause_e = w, *line_e = w, *sentence_e = w,
         *paragraph_e = w, *page_e = w;
    while (w != NULL)
    {
        Word *n = next_word(w);
        if (n == NULL || n->clause_start    != w->clause_start)    clause_e    = w;
        if (n == NULL || n->sentence_start  != w->sentence_start)  sentence_e  = w;
        if (n == NULL || n->paragraph_start != w->paragraph_start) paragraph_e = w;
        if (n == NULL || n->line_start      != w->line_start)      line_e      = w;
        if (n == NULL || n->page_start      != w->page_start)      page_e      = w;
        w->clause_end    = clause_e;
        w->sentence_end  = sentence_e;
        w->paragraph_end = paragraph_e;
        w->line_end      = line_e;
        w->page_end      = page_e;
        w = prev_word(w);
    }
}

void
print_words(Word *list)
{
    WordIterator iterator = init_word_iterator(list, next_word, false);
    while (iterator_has_next_word(iterator) == true)
    {
        Word *current = iterator_next_word(&iterator);
        printf("%10zu: '%s' ('%s')", position_word(current), original_word(current), reduced_word(current));
        if (sentence_ending_word(current) == true)
        {
            printf(" ...");
        }
        printf("\n");
    }
}

void
free_words(Word *list)
{
    WordIterator iterator = init_word_iterator(list, next_word, false);
    while (iterator_has_next_word(iterator) == true)
    {
        Word *current = iterator_next_word(&iterator);
        free(current->original);
        free(current->reduced);
        free(current);
    }
}

WordIterator
init_word_iterator(Word *word, Word *direction_word(Word *), bool limit_to_field)
{
    unsigned long prev_field = field_word(word); /* This ensures that it iterates at least once. */
    WordIterator iterator = {word, direction_word, limit_to_field, prev_field};
    return iterator;
}

Word *
iterator_next_word(WordIterator *iterator)
{
    Word *next = iterator->next;
    iterator->prev_field = field_word(next);
    iterator->next = iterator->direction_word(next);
    return next;
}

bool
iterator_has_next_word(WordIterator iterator)
{
    if ((iterator.limit_to_field == true) && (iterator.prev_field != field_word(iterator.next)))
    {
        return false;
    }
    else
    {
        return (iterator.next != NULL);
    }
}
