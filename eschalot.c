/* eschalot - generates vanity .onion names using brute-force hashing.
 * A fork of shallot, which was a fork of onionhash. */

/*
 * Copyright (c) 2013 Unperson Hiro <blacksunhq56imku.onion>
 * Copyright (c) 2007 Orum          <hangman5naigg7rr.onion>
 * Copyright (c) 2006 Cowboy Bebop  <torlandypjxiligx.onion>
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

/* Lets agree on 100 characters line limit, tab is 8 spaces long, second level ident is 4 spaces. */
/* ---------- This wide ------------------------------------------------------------------------- */

#ifdef __linux__
# define _GNU_SOURCE
# include <endian.h>
#endif

#ifdef __FreeBSD__
# include <sys/endian.h>
#endif

#include <sys/types.h>

#include <ctype.h>
#include <getopt.h>
#include <inttypes.h>
#include <pthread.h>
#include <regex.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <openssl/bn.h>
#include <openssl/pem.h>
#include <openssl/rsa.h>
#include <openssl/sha.h>

/* Define NEED_HTOBE32 if htobe32() is not available on your platform. */
/* #define NEED_HTOBE32 */
#if BYTE_ORDER == LITTLE_ENDIAN
# ifdef NEED_HTOBE32
#  define HTOBE32(x)	(((uint32_t)(x) & 0xffu)	<< 24 |	\
			 ((uint32_t)(x) & 0xff00u)	<<  8 |	\
			 ((uint32_t)(x) & 0xff0000u)	>>  8 |	\
			 ((uint32_t)(x) & 0xff000000u)	>> 24)
# else
#  define HTOBE32(x)	htobe32(x)
# endif
#else
# define HTOBE32(x)	(x)
#endif

/* Define NEED_STRNLEN if strnlen() is not available on your platform. */
/* #define NEED_STRNLEN */
#ifdef NEED_STRNLEN
static size_t		strnlen(const char *, size_t);
size_t
strnlen(const char *s, size_t ml)
{
	unsigned int i;
	for (i = 0; *(s + i) != '\0' && i < ml; i++)
		;
	return i;
}
#endif


#define VERSION		"1.2.0"
#define SHA_REL_CTX_LEN	10 * sizeof(SHA_LONG)	/* 40 bytes */
#define RSA_KEYS_BITLEN	1024			/* RSA key length to use */
#define SIZE_OF_E	4			/* Limit public exponent to 4 bytes */
#define RSA_E_START	0xFFFFFFu + 2		/* Min e */
#define RSA_E_LIMIT	0xFFFFFFFFu		/* Max e */
#define ONION_LENP1	17			/* Onion name length plus 1 */
#define MAX_THREADS	100			/* Maximum number of threads */
#define MAX_WORDS	0xFFFFFFFFu		/* Maximum words to read from file */
#define BASE32_ALPHABET	"abcdefghijklmnopqrstuvwxyz234567"

extern char	*__progname;

/* Error and debug functions */
static void		usage(void);
static void		error(char *, ...);
static void		verbose(char *, ...);
static void		normal(char *, ...);
static void		(*msg)(char *, ...);
/* User IO functions */
static void		setoptions(int, char *[]);
static void		readfile(void);
static void		printresult(RSA *, uint8_t *, uint8_t *);
/* Math and search functions */
static _Bool		fsearch(uint8_t *, uint8_t *);
static _Bool		psearch(uint8_t *, uint8_t *);
static _Bool		rsearch(uint8_t *, uint8_t *);
static _Bool		(*search)(uint8_t *, uint8_t *);
static _Bool		validkey(RSA *);
static signed int	compare(const void *, const void *);
static void		base32_enc(uint8_t *, uint8_t *);
static void		base32_dec(uint8_t *, uint8_t *);
static void		onion_enc(uint8_t *, RSA *);
static void		zerobits(uint16_t * ind, uint64_t * word,
				 uint8_t * buffer, unsigned int length);
/* Main thread routine */
static void		*worker(void *);

/* Global variables */
/* TODO: perhaps getting rid of so many globals is in order... */
_Bool			done, cflag, fflag, nflag, pflag, rflag, vflag;
unsigned int		minlen, maxlen, threads, prefixlen, wordcount;
char			fn[FILENAME_MAX + 1], prefix[ONION_LENP1];
regex_t			*regex;

struct {
	unsigned int	 count;
	uint64_t	*branch;
} tree[65536] = { {0, NULL} };

int
main(int argc, char *argv[])
{
	pthread_t	babies[MAX_THREADS];
	uint64_t	count[MAX_THREADS];
	unsigned int	i, j, dupcount = 0;

	/* Initialize global flags */
	wordcount = done = cflag = fflag = nflag = pflag = rflag = vflag = 0;
	minlen = 8;
	maxlen = 16;
	threads = 1;
        msg = normal;	/* Default: non-verbose */
        search = NULL;	/* No default search, has to be specified */
	
	setoptions(argc, argv);

	if (fflag) {
		readfile();
		msg("Sorting the word hashes and removing duplicates.\n");
		wordcount = 0;
		for (i = 0; i < 65536; i++) {
			dupcount = 0;
			qsort(tree[i].branch, tree[i].count, sizeof(tree[i].branch[0]), &compare);
			for (j = 1; j < tree[i].count ; j++) {
				if (tree[i].branch[j] == tree[i].branch[j - 1]) {
					tree[i].branch[j - 1] = 0xFFFFFFFFFFFFFFFFu;
					dupcount++;
				}
			}

			if (dupcount > 0) {
				qsort(tree[i].branch, tree[i].count, sizeof(tree[i].branch[0]),
				    &compare);
				tree[i].count -= dupcount;
			}
			wordcount += tree[i].count;
		}
		msg("Final word count: %d.\n", wordcount);
	}

	/* Start our threads */
	for (i = 1; i <= threads; i++) {
		count[i] = 0;
		if (pthread_create(&babies[i], NULL, worker, (void *)&count[i]) != 0)
			error("Failed to start thread!\n");
		msg("Thread #%d started.\n", i);
	}

	/* Monitor performance
	 * TODO: redo this whole thing, add estimate time for keys */
	if (vflag) {
		uint64_t	loops;
		time_t		elapsed, start = time(NULL);
		unsigned int	delay = 5;

		msg("Running, collecting performance data...\n");
		for(;;) {
			sleep(delay *= 2);
			if (done)
				return 0;

			/* Collect our thread's counters */
			loops = 0;
			i = threads;
			do {
				loops += count[i];
			} while (--i);

			/* On a really slow machine the initial delay might not be
			 * enough to generate the first RSA key, so give it more time. */
			if (loops == 0)
				continue;

			elapsed = time(NULL) - start;

			/* Bug here somewhere - with pcc compiler only. */
			/* TODO: Fix. */
			msg("Total hashes: %" PRIu64
			    ", running time: %d seconds, hashes per second: %" PRIu64 "\n",
			    loops, elapsed, (uint64_t)(loops / elapsed));
		}
	}
	/* Wait for all the threads to exit */
	for (i = 1; i <= threads; i++)
		pthread_join(babies[i], NULL);
	exit(0);
}

/* Main hashing thread */
void *
worker(void *arg)
{
	SHA_CTX		hash, copy;
	RSA		*rsa;
	uint8_t		*tmp, *der,
			buf[SHA_DIGEST_LENGTH],
			onion[ONION_LENP1],
			onionfinal[ONION_LENP1];
	signed int	derlen;
	uint64_t	*counter;
	/* Public exponent and the "big-endian" version of it */
	unsigned int	e, e_be;

	counter = (uint64_t *)arg;

	while (!done) {
		/* Generate a new RSA key every time e reaches RSA_E_LIMIT */
		rsa = RSA_generate_key(RSA_KEYS_BITLEN, RSA_E_START,
		    NULL, NULL);
		if (!rsa)
			error("RSA Key Generation failed!\n");
		
		/* Too chatty - disable. */
		/* msg("Generated a new RSA key pair.\n"); */
		
		/* Encode RSA key in X.690 DER format */
		if((derlen = i2d_RSAPublicKey(rsa, NULL)) < 0)
			error("DER encoding failed!\n");
		if ((der = tmp = (uint8_t *)malloc(derlen)) == NULL)
			error("malloc(derlen) failed!\n");
		if (i2d_RSAPublicKey(rsa, &tmp) != derlen)
			error("DER encoding failed!\n");

		/* Prepare the hash context */
		SHA1_Init(&hash);
		SHA1_Update(&hash, der, derlen - SIZE_OF_E);
		free(der);
		e = RSA_E_START - 2; /* public exponent */

		/* Main loop */
		while  ((e < RSA_E_LIMIT) && !done) {
			e += 2;
			/* Convert e to big-endian format. */
			e_be = HTOBE32(e);
			/* Copy the relevant parts of already set up context. */
			memcpy(&copy, &hash, SHA_REL_CTX_LEN); /* 40 bytes */
			copy.num = hash.num;
			/* Compute SHA1 digest (the real bottleneck) */
			SHA1_Update(&copy, &e_be, SIZE_OF_E);
			SHA1_Final(buf, &copy);
			(*counter)++;
			/* This is fairly fast, but can be faster if inlined. */
			base32_enc(onion, buf);

			/* The search speed varies based on the mode we are in.
			 * Regex's performance depends on the expression used.
			 * Fixed prefix is as fast as memcmp(3).
			 * Wordlist performance depends on (mostly):
			 *   number of "lengths" to search for (-l from-to);
			 *   number of unique words loaded from file.
			 *
			 * Inlining everything and optimizing for one mode and
			 * fixed word length improved the performance somewhat
			 * when I tried it, but it's not worth it. */
			if (search(buf, onion)) {
				/* Found a possible key,
				 * from here on down performance is not critical. */
				if (!BN_bin2bn((uint8_t *)&e_be, SIZE_OF_E, rsa->e))
					error("Failed to set e in RSA key!\n");
				if (!validkey(rsa))
					error("A bad key was found!\n");
				if (pflag)
					onion[prefixlen] = '\0';

				onion_enc(onionfinal, rsa);

				/* Every so often the onion found matches
				 * whatever we were looking for, but the final
				 * generated onion is garbage. I suspect a CPU
				 * or RAM overheating, but it could be a subtle
				 * bug somewhere. Hard to pin-point. According
				 * to the reports I've seen, shallot has had a
				 * similar problem.
				 *
				 * Happens most often in a wordlist search mode,
				 * but I think I have seen it in a regex mode
				 * as well. Does not seem to happen in a fixed
				 * prefix mode.
				 *
				 * Adding this check to avoid producing garbage
				 * results and to alleviate the problem a bit. */
				if (strncmp((char *)onion, (char *)onionfinal,
				    (!rflag ? strnlen((char *)onion, ONION_LENP1) : 16))) {
					msg("\nWARNING! Error detected! CPU/RAM overheating?\n");
					msg("Found %s, but finalized to %s.\n",
					    (char *)onion, (char *)onionfinal);
					msg("Suspending this thread for 30 seconds.\n");
					sleep(30);
					msg("Generating new RSA key.\n\n");
					break;
				} else
					printresult(rsa, onion, onionfinal);
				
				if (!cflag)
					done = 1; /* Notify other threads. */
				break;
			}
		}
		RSA_free(rsa);
	}
	return 0;
}

/* Read words from file */
void
readfile(void)
{
	FILE		*file;
	uint8_t		w[ONION_LENP1] = { 0 }, buf[10];
	uint8_t		len, j;
	uint16_t	ind;
	signed int	c;
	uint64_t	wrd;
	_Bool		inword;

	if ((file = fopen(fn,"r")) == NULL)
		error("Failed to open %s!\n", fn);
	msg("Reading words from %s, please wait...\n", fn);

	/* We have to inspect each character individually anyway,
	 * so lets just use fgetc here and let the OS buffer stuff. */
	j = 0;
	while ((c = fgetc(file)) != EOF) {
		c = tolower(c);
		inword = 0;
		/* Only load words with digits if the -n option was used. */
		if ((c >= 'a' && c <= 'z') || (nflag && c >= '2' && c <= '7')) {
				w[j++] = c;
				inword = 1;
			}

		if ((!inword && j > 0) || j > 16) {
			/* We clip the words longer than 16 characters here. */
			w[j] = '\0';
			j = 0;
			/* Only pick the words of the length we need. */
			len = strnlen((char *)w, ONION_LENP1);
			if (len >= minlen && len <= maxlen && wordcount < MAX_WORDS) {
				base32_dec(buf, w);
				memset(w, 0, sizeof(w));
				zerobits(&ind, &wrd, buf, len);

				if ((tree[ind].branch = (uint64_t *)realloc(tree[ind].branch,
				    sizeof(uint64_t) * (tree[ind].count + 1))) == NULL)
					error("realloc() failed!\n");

				tree[ind].branch[tree[ind].count] = wrd;
				tree[ind].count++; 
				wordcount++;
			}
		}
	}
	fclose(file);

	if (wordcount == 0 )
		error("Could not find any valid words in %s!\n", fn);
	else
		msg("Loaded %d words.\n", wordcount);
}

/* Check if the RSA key is ok (PKCS#1 v2.1). */
_Bool	
validkey(RSA *rsa)
{
	BN_CTX	*ctx = BN_CTX_new();
	BN_CTX_start(ctx);
	BIGNUM	*p1 = BN_CTX_get(ctx),		/* p - 1 */
		*q1 = BN_CTX_get(ctx),		/* q - 1 */
		*gcd = BN_CTX_get(ctx),		/* GCD(p - 1, q - 1) */
		*lambda = BN_CTX_get(ctx),	/* LCM(p - 1, q - 1) */
		*tmp = BN_CTX_get(ctx);		/* temporary storage */

	BN_sub(p1, rsa->p, BN_value_one());	/* p - 1 */
	BN_sub(q1, rsa->q, BN_value_one());	/* q - 1 */
	BN_gcd(gcd, p1, q1, ctx);	   	/* gcd(p - 1, q - 1) */

	BN_div(tmp, NULL, p1, gcd, ctx);
	BN_mul(lambda, q1, tmp, ctx);		/* lambda(n) */

	/* Check if e is coprime to lambda(n). */
	BN_gcd(tmp, lambda, rsa->e, ctx);
	if (!BN_is_one(tmp)) {
		verbose("WARNING: Key check failed - e is coprime to lambda!\n");
		return 0;
	}

	/* Check if public exponent e is less than n - 1. */
	/* Subtract n from e to avoid checking BN_is_zero. */
	BN_sub(tmp, rsa->e, rsa->n);
	if (!tmp->neg) {
		verbose("WARNING: Key check failed - e is less than (n - 1)!\n");
		return 0;
	}

	BN_mod_inverse(rsa->d, rsa->e, lambda, ctx);	/* d */
	BN_mod(rsa->dmp1, rsa->d, p1, ctx);		/* d mod(p - 1) */
	BN_mod(rsa->dmq1, rsa->d, q1, ctx);		/* d mod(q - 1) */
	BN_mod_inverse(rsa->iqmp, rsa->q, rsa->p, ctx);	/* q ^ -1 mod p */
	BN_CTX_end(ctx);
	BN_CTX_free(ctx);

	/* In theory this should never be true,
	 * unless the guy before me made a mistake ;). */
	if (RSA_check_key(rsa) != 1) {
		verbose("WARNING: OpenSSL's RSA_check_key(rsa) failed!\n");
		return 0;
	}
	return 1;
}

/* Base32 encode 10 byte long 'src' into 16 character long 'dst' */
/* Experimental, unroll everything. So far, it seems to be the fastest of the
 * algorithms that I've tried. TODO: review and decide if it's final.*/
void
base32_enc (uint8_t *dst, uint8_t *src)
{
	dst[ 0] = BASE32_ALPHABET[ (src[0] >> 3)			    ];
	dst[ 1] = BASE32_ALPHABET[((src[0] << 2) | (src[1] >> 6))	& 31];
	dst[ 2] = BASE32_ALPHABET[ (src[1] >> 1) 			& 31];
	dst[ 3] = BASE32_ALPHABET[((src[1] << 4) | (src[2] >> 4))	& 31];
	dst[ 4] = BASE32_ALPHABET[((src[2] << 1) | (src[3] >> 7))	& 31];
	dst[ 5] = BASE32_ALPHABET[ (src[3] >> 2)			& 31];
	dst[ 6] = BASE32_ALPHABET[((src[3] << 3) | (src[4] >> 5))	& 31];
	dst[ 7] = BASE32_ALPHABET[  src[4]				& 31];

	dst[ 8] = BASE32_ALPHABET[ (src[5] >> 3)			    ];
	dst[ 9] = BASE32_ALPHABET[((src[5] << 2) | (src[6] >> 6))	& 31];
	dst[10] = BASE32_ALPHABET[ (src[6] >> 1)			& 31];
	dst[11] = BASE32_ALPHABET[((src[6] << 4) | (src[7] >> 4))	& 31];
	dst[12] = BASE32_ALPHABET[((src[7] << 1) | (src[8] >> 7))	& 31];
	dst[13] = BASE32_ALPHABET[ (src[8] >> 2)			& 31];
	dst[14] = BASE32_ALPHABET[((src[8] << 3) | (src[9] >> 5))	& 31];
	dst[15] = BASE32_ALPHABET[  src[9]				& 31];

	dst[16] = '\0';
}

/* Decode base32 16 character long 'src' into 10 byte long 'dst'. */
/* TODO: Revisit and review, would like to shrink it down a bit.
 * However, it has to stay endian-safe and be fast. */
void
base32_dec (uint8_t *dst, uint8_t *src)
{
	uint8_t		tmp[ONION_LENP1];
	unsigned int	i;

	for (i = 0; i < 16; i++) {
		if (src[i] >= 'a' && src[i] <= 'z') {
			tmp[i] = src[i] - 'a';
		} else {
			if (src[i] >= '2' && src[i] <= '7')
				tmp[i] = src[i] - '1' + ('z' - 'a');
		 	else {
				/* Bad character detected.
				 * This should not happen, but just in case
				 * we will replace it with 'z' character. */
				tmp[i] = 26;
			}
		}
	}
	dst[0] = (tmp[ 0] << 3) | (tmp[1] >> 2);
	dst[1] = (tmp[ 1] << 6) | (tmp[2] << 1) | (tmp[3] >> 4);
	dst[2] = (tmp[ 3] << 4) | (tmp[4] >> 1);
	dst[3] = (tmp[ 4] << 7) | (tmp[5] << 2) | (tmp[6] >> 3);
	dst[4] = (tmp[ 6] << 5) |  tmp[7];
	dst[5] = (tmp[ 8] << 3) | (tmp[9] >> 2);
	dst[6] = (tmp[ 9] << 6) | (tmp[10] << 1) | (tmp[11] >> 4);
	dst[7] = (tmp[11] << 4) | (tmp[12] >> 1);
	dst[8] = (tmp[12] << 7) | (tmp[13] << 2) | (tmp[14] >> 3);
	dst[9] = (tmp[14] << 5) |  tmp[15];
}

/* Print found .onion name and PEM formatted RSA key. */
void 
printresult(RSA *rsa, uint8_t *target, uint8_t *actual)
{	
	uint8_t		*dst;
	BUF_MEM		*buf;
	BIO		*b;

	b = BIO_new(BIO_s_mem());

	PEM_write_bio_RSAPrivateKey(b, rsa, NULL, NULL, 0, NULL, NULL);
	BIO_get_mem_ptr(b, &buf);
	(void)BIO_set_close(b, BIO_NOCLOSE);
	BIO_free(b);

	if ((dst = (uint8_t *)malloc(buf->length + 1)) == NULL)
		error("malloc(buf->length + 1) failed!\n");
	memcpy(dst, buf->data, buf->length);

	dst[buf->length] = '\0';
	
	msg("Found a key for %s (%d) - %s.onion\n",
	    target, strnlen((char *)target, ONION_LENP1), actual);
	printf("----------------------------------------------------------------\n");
	printf("%s.onion\n", actual);
	printf("%s\n", dst);
	fflush(stdout);

	BUF_MEM_free(buf);
	free(dst);
}

/* Generate .onion name from the RSA key. */
/* (using the same method as the official TOR client) */
void
onion_enc(uint8_t *onion, RSA *rsa)
{
	uint8_t		*bufa, *bufb, digest[SHA_DIGEST_LENGTH];
	signed int	derlen;

	if((derlen = i2d_RSAPublicKey(rsa, NULL)) < 0)
		error("DER encoding failed!\n");

	if ((bufa = bufb = (uint8_t *)malloc(derlen)) == NULL)
		error("malloc(derlen) failed!\n");

	if (i2d_RSAPublicKey(rsa, &bufb) != derlen)
		error("DER encoding failed!\n");

	SHA1(bufa, derlen, digest);
	free(bufa);
	base32_enc(onion, digest);
}

/* Compare function for qsort(3). */
signed int
compare (const void *a, const void *b)
{
	if (*((const uint64_t*)a) > *((const uint64_t*)b))
		return 1;
	if (*((const uint64_t*)a) < *((const uint64_t*)b))
		return -1;
	return 0;
}

/* Wordlist mode search. */
_Bool	
fsearch(uint8_t *buf, uint8_t *onion)
{
	unsigned int i;
	uint16_t ind;
	uint64_t wrd;

	if (!nflag)
		for (i = 0; i < minlen; i++)
			if (onion[i] < 'a')
				return 0;

	for (i = minlen; i <= maxlen; i++) {
		if (!nflag && onion[i - 1] < 'a')
			return 0;

		zerobits(&ind, &wrd, buf, i);

		if (tree[ind].branch != NULL &&
		    bsearch(&wrd, tree[ind].branch, tree[ind].count,
			    sizeof(tree[ind].branch[0]), &compare)) {
			onion[i] = '\0';
			return 1;
		}
	}
	return 0;
}

/* Regex mode search. */
_Bool	
rsearch(__attribute__((unused)) uint8_t *buf, uint8_t *onion)
{
	return !regexec(regex, (char *)onion, 0, 0, 0);
}

/* Fixed prefix mode search. */
_Bool	
psearch(__attribute__((unused)) uint8_t *buf, uint8_t *onion)
{
	return !memcmp(onion, prefix, prefixlen);
}

/* Zero unused bits, split 10 byte 'buffer' into 2 byte 'ind' and 8 byte 'word'. */
/* TODO: currently doing '1' fill instead of zeroes - decide if it's final. */
void
zerobits(uint16_t * ind, uint64_t * word, uint8_t * buffer, unsigned int length)
{
	uint8_t bufcopy[10];
	unsigned int usedbytes, usedbits;

	memcpy(bufcopy, buffer, 10);
	usedbytes = length * 5 / 8;
	usedbits  = length * 5 % 8;

	if (usedbits) {
		/* Lets try '1' fill instead of zeroes.. */
		/* bufcopy[usedbytes] &= (0xFFu << (8 - usedbits)); */
		bufcopy[usedbytes] |= (0xFFu >> usedbits);
		usedbytes++;
	}

	if (usedbytes < 10) {
		/* Lets try '1' fill instead of zeroes.. */
		/* memset(&bufcopy[usedbytes], 0, 10 - usedbytes); */
		memset(&bufcopy[usedbytes], 0xFFu, 10 - usedbytes);
	}

	memcpy(ind,  &bufcopy[0], 2);
	memcpy(word, &bufcopy[2], 8);
}

/* Read arguments and set global variables. */
void
setoptions(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "cnvt:l:f:p:r:")) != -1)
		switch (ch) {
		case 'c':
			cflag = 1;
			break;
		case 'n':
			nflag = 1;
			break;
		case 'v':
			vflag = 1;
			msg = verbose;
			break;
		case 't':
			threads = strtoul(optarg, NULL, 0);
			if (threads < 1)
				usage();
			if (threads > MAX_THREADS)
				threads = MAX_THREADS;
			break;
		case 'l':
			minlen = strtoul(optarg, NULL, 0);
			if (minlen < 8 || minlen > 16 || !strchr(optarg, '-'))
				usage();
			maxlen = strtoul(strchr(optarg, '-') + 1, NULL, 0);
			if (maxlen < 8 || maxlen > 16 || minlen > maxlen)
				usage();
			break;
		case 'f':
			fflag = 1;
			strncpy(fn, optarg, FILENAME_MAX);
			search = fsearch;
			break;
		case 'p':
			pflag = 1;
			strncpy(prefix, optarg, ONION_LENP1);
			minlen = maxlen = prefixlen = strnlen(prefix, ONION_LENP1);
			if (prefixlen > 16 || prefixlen < 1)
				usage();
			for (unsigned int i = 0; i < prefixlen; i++) {
				prefix[i] = tolower(prefix[i]);
				if (!isalpha(prefix[i]) && (!nflag || !isdigit(prefix[i]) ||
				     prefix[i] < '2' || prefix[i] > '7'))
					usage();
			}
			search = psearch;
			break;
		case 'r':
			rflag = 1;
			minlen = 1;
			maxlen = 16;
			if ((regex = (regex_t *)malloc(sizeof(regex_t))) == NULL)
				error("malloc(sizeof(regex_t)) failed!\n");;

			/* Do not use ICASE - too slow. */
			if (regcomp(regex, optarg, REG_EXTENDED | REG_NOSUB))
				error("Failed to compile regex expression!\n");
			search = rsearch;
			break;
		default:
			usage();
			/* NOTREACHED */
		}

	if (fflag + rflag + pflag != 1)
		usage();

	msg("Verbose, ");
	cflag ?	msg("continuous, ") : msg("single result, ");
	nflag ?	msg("digits ok, ")  : msg("no digits, ");
	msg("%d threads, prefixes %d-%d characters long.\n",
	    threads, minlen, maxlen);
}

/* Print usage information and exit. */
void
usage(void)
{
	fprintf(stderr,
	    "Version: %s\n"
	    "\n"
	    "usage:\n"
	    "%s [-c] [-v] [-t count] ([-n] [-l min-max] -f filename) | (-r regex) | (-p prefix)\n"
	    "  -v         : verbose mode - print extra information to STDERR\n"
	    "  -c         : continue searching after the hash is found\n"
	    "  -t count   : number of threads to spawn default is one)\n"
	    "  -l min-max : look for prefixes that are from 'min' to 'max' characters long\n"
	    "  -n         : Allow digits to be part of the prefix (affects wordlist mode only)\n"
	    "  -f filename: name of the text file with a list of prefixes\n"
	    "  -p prefix  : single prefix to look for (1-16 characters long)\n"
	    "  -r regex   : search for a POSIX-style regular expression\n"
	    "\n"
	    "Examples:\n"
	    "  %s -cvt4 -l8-12 -f wordlist.txt >> results.txt\n"
	    "  %s -v -r '^test|^exam'\n"
	    "  %s -ct5 -p test\n\n",
	    VERSION, __progname, __progname, __progname, __progname);

	fprintf(stderr,
	    "  base32 alphabet allows letters [a-z] and digits [2-7]\n"
	    "  Regex pattern examples:\n"
	    "    xxx           must contain 'xxx'\n"
	    "    ^foo          must begin with 'foo'\n"
	    "    bar$          must end with 'bar'\n"
	    "    b[aoeiu]r     must have a vowel between 'b' and 'r'\n"
	    "    '^ab|^cd'     must begin with 'ab' or 'cd'\n"
	    "    [a-z]{16}     must contain letters only, no digits\n"
	    "    ^dusk.*dawn$  must begin with 'dusk' and end with 'dawn'\n"
	    "    [a-z2-7]{16}  any name - will succeed after one iteration\n");

	exit(1);
}

/* Spam STDERR with diagnostic messages... */
void
verbose(char *message, ...)
{
	va_list	ap;

	va_start(ap, message);
	vfprintf(stderr, message, ap);
	va_end(ap);
	fflush(stderr);
}

/* ...or be a very quiet thinker. */
void
normal(__attribute__((unused)) char *unused, ...)
{
}

/* Print error message and exit. */
/* (Not all Linuxes implement the err/errx functions properly.) */
void
error(char *message, ...)
{
	va_list	ap;

	va_start(ap, message);
	fprintf(stderr, "ERROR: ");
	vfprintf(stderr, message, ap);
	va_end(ap);
	exit(1);
}

