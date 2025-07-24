/* RadioJKK32 - Multifunction Internet Radio Player
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * Utility tools and text processing functions
*/

#pragma once

#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Convert UTF-8 Polish text to ASCII equivalent
 * @param input Input UTF-8 string with Polish diacritics
 * @param output Output buffer for ASCII text (NULL to modify input in-place)
 * 
 * Converts Polish diacritical characters (ą, ć, ę, ł, ń, ó, ś, ź, ż) 
 * to their ASCII equivalents (a, c, e, l, n, o, s, z, z).
 * If output is NULL, the conversion is done in-place on the input string.
 */
void Utf8ToAsciiPL(const char *input, char *output);

#ifdef __cplusplus
}
#endif