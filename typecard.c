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

#include <limits.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>


/* Check Endianness */
#if   __BYTE_ORDER__ == __ORDER_LITTLE_ENDIAN__
#	define ENDIAN_TYPE "Little"
#elif __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#	define ENDIAN_TYPE "Big"
#elif __BYTE_ORDER__ == __ORDER_PDP_ENDIAN__
#	define ENDIAN_TYPE "PDP"
#else
#	define ENDIAN_TYPE "Unknown"
#endif


/* Print the name and size of a type */
static void
print_type_size(char *type, size_t size) {
	printf("%s:\t%2d (%3d bits)\n", type, size, size * CHAR_BIT);
}

int
main() {
	printf("\n");
        printf("System: %d bit\n", sizeof(void*) * CHAR_BIT);

	/* possible endian types */
	printf("Host byte order: %s Endian\n", ENDIAN_TYPE);
	printf("\n");

	print_type_size("char\t",      sizeof(char));
	print_type_size("short\t",     sizeof(short));
	print_type_size("int\t",       sizeof(int));
	print_type_size("long\t",      sizeof(long));
	print_type_size("long long", sizeof(long long));
	putchar('\n');

	print_type_size("float\t",       sizeof(float));
	print_type_size("double\t",      sizeof(double));
	print_type_size("long double", sizeof(long double));
	putchar('\n');

	print_type_size("wchar_t", sizeof(wchar_t));
	print_type_size("size_t\t",  sizeof(size_t));
	print_type_size("_Bool\t",   sizeof(_Bool));
	putchar('\n');

	print_type_size("int8_t\t",  sizeof(int8_t));
	print_type_size("int16_t", sizeof(int16_t));
	print_type_size("int32_t", sizeof(int32_t));
	print_type_size("int64_t", sizeof(int64_t));
#ifdef __sizeof_int128__
	print_type_size("__int128_t", sizeof(__int128_t));
#endif
	putchar('\n');

	print_type_size("int_least8_t",  sizeof(int_fast8_t));
	print_type_size("int_least16_t", sizeof(int_fast16_t));
	print_type_size("int_least32_t", sizeof(int_fast32_t));
	print_type_size("int_least64_t", sizeof(int_fast64_t));
	putchar('\n');

	print_type_size("int_fast8_t",  sizeof(int_fast8_t));
	print_type_size("int_fast16_t", sizeof(int_fast16_t));
	print_type_size("int_fast32_t", sizeof(int_fast32_t));
	print_type_size("int_fast64_t", sizeof(int_fast64_t));
	putchar('\n');

	print_type_size("intmax_t", sizeof(intmax_t));
	print_type_size("intptr_t", sizeof(intptr_t));
}

