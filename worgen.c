/* worgen - generates word combinations out of one to three word lists.
 * A helper utility for the eschalot tor names generator. */

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
#include <getopt.h>
#include <inttypes.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "1.2.0"

/* constants */
#define MAX_WORDS	0xFFFFFFFFu
#define MAX_LEN		16
#define BASE32_ALPHABET	"abcdefghijklmnopqrstuvwxyz234567"

extern	char		*__progname;

struct	f {
	unsigned int minlen;
	unsigned int maxlen;
	unsigned int wordcount;
	char fn[FILENAME_MAX + 1];
	char **words;
};

void	readfile(struct f *);
void	usage(void);
void	error(char *, ...);
void	verbose(char *, ...);
void	normal(char *, ...);
void	(*msg)(char *, ...) = verbose;
size_t	mystrnlen(const char *, size_t);

int
main(int argc, char *argv[])
{
	struct		f w[4];
	signed int	lists = -1;
	unsigned int	i, j, k, len1, len2, len3;

	/* Arguments processing */
	if (argc != 4 && argc != 6 && argc != 8)
		usage();

	for (i = 0; i < (unsigned int)argc; i += 2) {
		j = i / 2;
		strncpy(w[j].fn, argv[i], FILENAME_MAX);
		w[j].minlen = strtoul(argv[i + 1], NULL, 0);
		w[j].maxlen = strtoul(strchr(argv[i + 1], '-') + 1, NULL, 0);
		w[j].wordcount = 0;
		w[j].words = NULL;
		lists++;
		if (i == 0)
			msg("Will be producing %d-%d character long word combinations.\n",
			    w[j].minlen, w[j].maxlen, w[j].fn);
		else
			msg("Reading %d-%d characters words from %s.\n",
			    w[j].minlen, w[j].maxlen, w[j].fn);

		/* Basic sanity checks */
		if (!w[j].fn || !w[j].minlen || !w[j].maxlen || w[j].maxlen > MAX_LEN)
			usage();
	}

	/* Read words from files */
	for (i = 1; i <= (unsigned int)lists; i++) {
		readfile(&w[i]);
	}

	/* Mix it up */
	for (i = 0; i < w[1].wordcount; i++) {
		fprintf(stderr, "\rWorking. %d%% complete, %d words (approximately %dMb) produced.",
		(i + 1) * 100 / w[1].wordcount, w[0].wordcount,
		(w[0].wordcount /1024 / 1024 * ((w[0].minlen + w[0].maxlen) / 2 + 1)));
		fflush(stderr);

		len1 = mystrnlen(w[1].words[i], MAX_LEN);
		if (len1 < w[1].minlen || len1 > w[1].maxlen)
			continue;

		if (len1 >= w[0].minlen && len1 <= w[0].maxlen) {
			printf("%s\n", w[1].words[i]);
			w[0].wordcount++;
		}

		for (j = 0; j < w[2].wordcount && lists > 1; j++) {
			len2 = mystrnlen(w[2].words[j], MAX_LEN);
			if (len2 < w[2].minlen || len2 > w[2].maxlen)
				continue;

			if (len1 + len2 >= w[0].minlen && len1 + len2 <= w[0].maxlen) {
				printf("%s%s\n", w[1].words[i], w[2].words[j]);
				w[0].wordcount++;
			}

			for (k = 0; k < w[3].wordcount && lists > 2; k++) {
				len3 = mystrnlen(w[3].words[k], MAX_LEN);
				if (len3 < w[3].minlen || len3 > w[3].maxlen)
					continue;

				if (len1 + len2 + len3 >= w[0].minlen && len1 + len2 + len3 <= w[0].maxlen) {
					printf("%s%s%s\n", w[1].words[i], w[2].words[j], w[3].words[k]);
					w[0].wordcount++;
				}
			}
		}
	}
	msg("\nFinal count: %d word combinations.\n", w[0].wordcount);
}

void
usage(void)
{
	fprintf(stderr,
	    "Version: %s\n"
	    "\n"
	    "usage: %s min-max filename1 min1-max1 [filename2 min2-max2 [filename3 min3-max3]]\n"
	    "  min-max   : length limits for the output strings\n"
	    "  filename1 : name of the first word list file (required)\n"
	    "  min1-max1 : length limits for the words from the first file\n"
	    "  filename2 : name of the second word list file (optional)\n"
	    "  min2-max2 : length limits for the words from the first file\n"
	    "  filename3 : name of the third word list file (optional)\n"
	    "  min3-max3 : length limits for the words from the first file\n\n"
	    "  Example: %s 8-12 wordlist1.txt 5-10 wordlist2.txt 3-5 > results.txt\n\n"
	    "              Generates word combinations from 8 to 12 characters long\n"
	    "              using 5-10 character long words from 'wordlist1.txt'\n"
	    "              followed by 3-5 character long words from 'wordlist2.txt'.\n"
	    "              Saves the results to 'results.txt'.\n",
	    VERSION, __progname, __progname);
	exit(1);
}

/* Read words from file */
void
readfile(struct f *list)
{
	FILE		*file;
	signed int	c;
	char		word[MAX_LEN + 1];
	unsigned int	len, i, j = 0;
	_Bool		inword;

	if ((file = fopen(list->fn, "r")) == NULL)
		error("Failed to open %s!\n", list->fn);

	msg("Loading words from %s.\n", list->fn);

	while ((c = fgetc(file)) != EOF) {
		c = tolower(c);
		inword = 0;
		for (i = 0; i < 32; i++) {
			if (c == BASE32_ALPHABET[i]) {
				word[j++] = c;
				inword = 1;
				break;
			}
		}

		if ((!inword && j > 0) || j > MAX_LEN) {
			word[j] = '\0';
			j = 0;
			len = mystrnlen(word, MAX_LEN);
			if (len >= list->minlen &&
			    len <= list->maxlen &&
			    list->wordcount < MAX_WORDS) {
				if ((list->words =
				    (char **)realloc(list->words,
				    (list->wordcount + 1) * sizeof(char *))) == NULL)
					error("realloc() failed!\n");

				if ((list->words[list->wordcount] =
				    (char *)malloc(mystrnlen(word, MAX_LEN) + 1)) == NULL)
					error("malloc() failed!\n");

				strncpy(list->words[list->wordcount], word, mystrnlen(word, MAX_LEN) + 1);
				list->wordcount++;
			}
		}
	}
	fclose(file);
	if (list->wordcount == 0)
		error("Could not find any valid words!\n");
	else
		msg("Loaded %d words from %s.\n", list->wordcount, list->fn);
}

/* Print error message and exit. Not using the err/errx,
 * since not all Linuxes implement them properly. */
void
error(char *message, ...)
{
	va_list	ap;

	va_start(ap, message);
	fprintf(stderr, "ERROR: ");
	vfprintf(stderr, message, ap);
	va_end(ap);
	fflush(stderr);
	exit(1);
}

/* Spam STDERR with diagnostic messages... */
void
verbose(char *message, ...) {
	va_list ap;
	va_start(ap, message);
	vfprintf(stderr, message, ap);
	va_end(ap);
	fflush(stderr);
}

/* ...or be a very quiet thinker. */
void
normal(__attribute__((unused)) char *notused, ...) {
}

/* Local version of strnlen function for older OSes. */
size_t
mystrnlen(const char *s, size_t ml)
{
	unsigned int i;
	for (i = 0; *(s + i) != '\0' && i < ml; i++)
		;
	return i;

}

