#pragma once

/**
 * @file pgmspace.h
 * @brief Mock AVR/ESP flash-string header, under its real name.
 *
 * The host has one address space, so the qualifiers are empty and the _P
 * accessors are their ordinary counterparts.
 */

#include <cstring>

#ifndef PROGMEM
#define PROGMEM
#endif

#ifndef PGM_P
#define PGM_P const char*
#endif

#ifndef PSTR
#define PSTR(s) (s)
#endif

#ifndef memcpy_P
#define memcpy_P(dest, src, n) memcpy((dest), (src), (n))
#endif

#ifndef strcpy_P
#define strcpy_P(dest, src) strcpy((dest), (src))
#endif

#ifndef strncpy_P
#define strncpy_P(dest, src, n) strncpy((dest), (src), (n))
#endif

#ifndef strlen_P
#define strlen_P(s) strlen(s)
#endif

#ifndef strcmp_P
#define strcmp_P(a, b) strcmp((a), (b))
#endif

#ifndef pgm_read_byte
#define pgm_read_byte(addr) (*reinterpret_cast<const uint8_t*>(addr))
#endif

#ifndef pgm_read_word
#define pgm_read_word(addr) (*reinterpret_cast<const uint16_t*>(addr))
#endif

#ifndef pgm_read_dword
#define pgm_read_dword(addr) (*reinterpret_cast<const uint32_t*>(addr))
#endif

#ifndef pgm_read_ptr
#define pgm_read_ptr(addr) (*reinterpret_cast<void* const*>(addr))
#endif
