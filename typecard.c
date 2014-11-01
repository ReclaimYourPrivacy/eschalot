/* typecard - prints basic C types and their sizes.
 * Used for the eschalot development and cross-platform testing. */

/*
 * Copyright (c) 2013 Unperson Hiro <blacksunhq56imku.onion>
 *
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <ctype.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int
main(int argc, char *argv[]) {
	printf("\n");
#ifdef __LP64__
        printf("System: 64 bit\n");
#else
        printf("System: 32 bit\n");
#endif

#if BYTE_ORDER == BIG_ENDIAN
        printf("Host byte order: Big Endian\n");
#endif
#if BYTE_ORDER == LITTLE_ENDIAN
        printf("Host byte order: Little Endian\n");
#endif
	printf("\n");
	printf("char:\t\t%2d (%3d bits)\n", sizeof(char), sizeof(char) * 8);
	printf("short:\t\t%2d (%3d bits)\n", sizeof(short), sizeof(short) * 8);
	printf("int:\t\t%2d (%3d bits)\n", sizeof(int), sizeof(int) * 8);
	printf("long:\t\t%2d (%3d bits)\n", sizeof(long), sizeof(long) * 8);
	printf("long long:\t%2d (%3d bits)\n", sizeof(long long), sizeof(long long) * 8);
	printf("\n");

	printf("float:\t\t%2d (%3d bits)\n", sizeof(float), sizeof(float) * 8);
	printf("double:\t\t%2d (%3d bits)\n", sizeof(double), sizeof(double) * 8);
	printf("long double:\t%2d (%3d bits)\n", sizeof(long double), sizeof(long double) * 8);
	printf("\n");

	printf("wchar_t:\t%2d (%3d bits)\n", sizeof(wchar_t), sizeof(wchar_t) * 8);
	printf("size_t:\t\t%2d (%3d bits)\n", sizeof(size_t), sizeof(size_t) * 8);
	printf("_Bool:\t\t%2d (%3d bits)\n", sizeof(_Bool), sizeof(_Bool) * 8);
	printf("\n");

	printf("int8_t:\t\t%2d (%3d bits)\n", sizeof(int8_t), sizeof(int8_t) * 8);
	printf("int16_t:\t%2d (%3d bits)\n", sizeof(int16_t), sizeof(int16_t) * 8);
	printf("int32_t:\t%2d (%3d bits)\n", sizeof(int32_t), sizeof(int32_t) * 8);
	printf("int64_t:\t%2d (%3d bits)\n", sizeof(int64_t), sizeof(int64_t) * 8);
#ifdef __LP64__
	printf("__int128_t:\t%2d (%3d bits)\n", sizeof(__int128_t), sizeof(__int128_t) * 8);
	printf("\n");
#endif
	printf("int_fast8_t:\t%2d (%3d bits)\n", sizeof(int_fast8_t), sizeof(int_fast8_t) * 8);
	printf("int_fast16_t:\t%2d (%3d bits)\n", sizeof(int_fast16_t), sizeof(int_fast16_t) * 8);
	printf("int_fast32_t:\t%2d (%3d bits)\n", sizeof(int_fast32_t), sizeof(int_fast32_t) * 8);
	printf("int_fast64_t:\t%2d (%3d bits)\n", sizeof(int_fast64_t), sizeof(int_fast64_t) * 8);
	printf("\n");
	printf("int_least8_t:\t%2d (%3d bits)\n", sizeof(int_least8_t), sizeof(int_least8_t) * 8);
	printf("int_least16_t:\t%2d (%3d bits)\n", sizeof(int_least16_t), sizeof(int_least16_t) * 8);
	printf("int_least32_t:\t%2d (%3d bits)\n", sizeof(int_least32_t), sizeof(int_least32_t) * 8);
	printf("int_least64_t:\t%2d (%3d bits)\n", sizeof(int_least64_t), sizeof(int_least64_t) * 8);
	printf("\n");
	printf("intmax_t:\t%2d (%3d bits)\n", sizeof(intmax_t), sizeof(intmax_t) * 8);
}


