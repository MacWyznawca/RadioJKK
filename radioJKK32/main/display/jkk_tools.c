/* 
 * Copyright (C) 2025 Jaromir Kopp (JKK)
 * 
 * Some tools 
 *
 */

#include <stdio.h>
#include "jkk_tools.h"

static const char *TAG = "JKK TOOLS";


void Utf8ToAsciiPL(const char *input, char *output) {
    const unsigned char *src = (const unsigned char *)input;
    char *dst = (char *)input;
    if(output != NULL) {
        dst = output;
    }

    while (*src) {
        unsigned char c1 = src[0];
        unsigned char c2 = src[1];

        // POLSKIE I ŁACIŃSKIE ZNAKI DIACENTRYCZNE
        if (c1 == 0xC4) {
            switch (c2) {
                case 0x84: *dst++ = 'A'; break; // Ą
                case 0x86: *dst++ = 'C'; break; // Ć
                case 0x98: *dst++ = 'E'; break; // Ę
                case 0x81: *dst++ = 'L'; break; // Ł
                case 0x83: *dst++ = 'N'; break; // Ń
                case 0x9A: *dst++ = 'S'; break; // Ś
                case 0xB9: *dst++ = 'Z'; break; // Ź
                case 0xBB: *dst++ = 'Z'; break; // Ż
                case 0x85: *dst++ = 'a'; break; // ą
                case 0x87: *dst++ = 'c'; break; // ć
                case 0x99: *dst++ = 'e'; break; // ę
                case 0x82: *dst++ = 'l'; break; // ł
                case 0x9B: *dst++ = 's'; break; // ś
                case 0xBA: *dst++ = 'z'; break; // ź
                case 0xBC: *dst++ = 'z'; break; // ż
                default: src += 2; continue;
            }
            src += 2;
        } else if (c1 == 0xC5) {
            switch (c2) {
                case 0x81: *dst++ = 'L'; break; // Ł
                case 0x82: *dst++ = 'l'; break; // ł
                case 0x83: *dst++ = 'N'; break; // Ń
                case 0x84: *dst++ = 'n'; break; // ń
                case 0x9A: *dst++ = 'S'; break; // Ś
                case 0x9B: *dst++ = 's'; break; // ś
                case 0xB9: *dst++ = 'Z'; break; // Ź
                case 0xBA: *dst++ = 'z'; break; // ź
                case 0xBB: *dst++ = 'Z'; break; // Ż
                case 0xBC: *dst++ = 'z'; break; // ż
                default: src += 2; continue;
            }
            src += 2;
        } else if (c1 == 0xC3) {
            switch (c2) {
                case 0x93: *dst++ = 'O'; break; // Ó
                case 0xB3: *dst++ = 'o'; break; // ó
                default: src += 2; continue;
            }
            src += 2;
        } else {
            *dst++ = *src++; // ASCII znak
        }
    }

    *dst = '\0';
}
