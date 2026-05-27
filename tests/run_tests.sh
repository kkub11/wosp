#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2025 Andrew Trettel

# Run from the project root regardless of where the script is invoked.
cd "$(dirname "$0")/.." || exit 1

WOSP="./wosp"
INPUT="A_Study_in_Scarlet.txt"
PASS=0
FAIL=0

check() {
    name="$1"
    expected="$2"
    actual="$3"
    if [ "$actual" = "$expected" ]; then
        echo "PASS: $name"
        PASS=$((PASS + 1))
    else
        echo "FAIL: $name"
        printf '  expected:\n'
        printf '%s\n' "$expected" | sed 's/^/    /'
        printf '  actual:\n'
        printf '%s\n' "$actual" | sed 's/^/    /'
        FAIL=$((FAIL + 1))
    fi
}

# Words used below (all occur once in A_Study_in_Scarlet.txt except bullet):
#   line 22: Candahar  (paragraph 1, ends "...new duties.")
#   line 28: Maiwand, Jezail, bullet
#   line 29: subclavian  (same sentence and clause-adjacent to Jezail)
#   line 30: Ghazis      (different sentence from Jezail, same paragraph)
#
# Before the symmetry fix, the proximity operators NEAR/AMONG/ALONG/WITH/SAME
# anchored the search window on the *first* operand only.  Reversing the
# operand order produced different (empty or wrong) results when the first
# word came *after* the second in the text, because advance_word() could not
# walk backwards past the start of the current element.

# ------------------------------------------------------------
# SAME (paragraph-level)
# Candahar is in paragraph 1 (line 22); Jezail is in paragraph 2 (line 28).
# Old code: 'Jezail SAME2 Candahar' returned nothing because Candahar lies
# before the start of Jezail's window (advance_word backward is stuck at the
# start of the current paragraph).
# ------------------------------------------------------------
SAME_EXPECTED="A_Study_in_Scarlet.txt:22:in reaching Candahar in safety, where I found my regiment, and at once
A_Study_in_Scarlet.txt:28:Maiwand. There I was struck on the shoulder by a Jezail bullet, which"

check "SAME2: forward operand order"   "$SAME_EXPECTED" "$($WOSP 'Candahar SAME2 Jezail'  $INPUT)"
check "SAME2: reversed operand order"  "$SAME_EXPECTED" "$($WOSP 'Jezail SAME2 Candahar'  $INPUT)"

# ------------------------------------------------------------
# ALONG (line-level)
# Jezail is on line 28; subclavian is on line 29 (consecutive lines).
# Old code: 'subclavian ALONG2 Jezail' returned nothing because Jezail lies
# before the start of subclavian's window.
#
# This test also exercises the consecutive-line output fix: the two matched
# words span different source lines and must appear on separate output lines
# with their own filename:lineno: prefixes, not squashed onto one line.
# ------------------------------------------------------------
ALONG_EXPECTED="A_Study_in_Scarlet.txt:28:Maiwand. There I was struck on the shoulder by a Jezail bullet, which
A_Study_in_Scarlet.txt:29:shattered the bone and grazed the subclavian artery. I should have"

check "ALONG2: forward operand order"   "$ALONG_EXPECTED" "$($WOSP 'Jezail ALONG2 subclavian'  $INPUT)"
check "ALONG2: reversed operand order"  "$ALONG_EXPECTED" "$($WOSP 'subclavian ALONG2 Jezail'  $INPUT)"

# Explicit check that consecutive source lines are NOT squashed into one line.
ALONG_LINECOUNT=$($WOSP 'Jezail ALONG2 subclavian' $INPUT | wc -l | xargs)
check "ALONG2: output preserves two source lines (not squashed)" "2" "$ALONG_LINECOUNT"

# ------------------------------------------------------------
# AMONG (clause-level)
# Jezail is in the clause "...by a Jezail bullet," (line 28);
# subclavian is in the next clause "...subclavian artery." (line 29).
# Old code: 'subclavian AMONG2 Jezail' returned nothing.
# ------------------------------------------------------------
AMONG_EXPECTED="$ALONG_EXPECTED"

check "AMONG2: forward operand order"   "$AMONG_EXPECTED" "$($WOSP 'Jezail AMONG2 subclavian'  $INPUT)"
check "AMONG2: reversed operand order"  "$AMONG_EXPECTED" "$($WOSP 'subclavian AMONG2 Jezail'  $INPUT)"

# ------------------------------------------------------------
# WITH (sentence-level)
# Jezail and subclavian are in the same sentence spanning lines 28-29.
# Old code: 'subclavian WITH1 Jezail' returned nothing.
# ------------------------------------------------------------
WITH_EXPECTED="$ALONG_EXPECTED"

check "WITH1: forward operand order"   "$WITH_EXPECTED" "$($WOSP 'Jezail WITH1 subclavian'  $INPUT)"
check "WITH1: reversed operand order"  "$WITH_EXPECTED" "$($WOSP 'subclavian WITH1 Jezail'  $INPUT)"

# ------------------------------------------------------------
# NEAR (word-level)
# Jezail and bullet are adjacent words on line 28.
# NEAR uses simple word-step navigation (prev_word), so it was already
# nearly symmetric, but this is a regression guard.
# ------------------------------------------------------------
NEAR_EXPECTED="A_Study_in_Scarlet.txt:28:Maiwand. There I was struck on the shoulder by a Jezail bullet, which"

check "NEAR1: forward operand order"   "$NEAR_EXPECTED" "$($WOSP 'Jezail NEAR1 bullet'  $INPUT)"
check "NEAR1: reversed operand order"  "$NEAR_EXPECTED" "$($WOSP 'bullet NEAR1 Jezail'  $INPUT)"

# ------------------------------------------------------------
# Summary
# ------------------------------------------------------------
echo ""
echo "Results: $PASS passed, $FAIL failed"
[ "$FAIL" -eq 0 ] && exit 0 || exit 1
