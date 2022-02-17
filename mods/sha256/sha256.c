/*
 * Copyright (c) 2021 Logan Ryan McLintock
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

/* SHA-256 algorithm */

/*
 * References:
 * FIPS PUB 180-4, Secure Hash Standard (SHS), National Institute of Standards
 *     and Technology, Maryland, August 2015.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../gen/gen.h"
#include "../fs/fs.h"
#include "sha256.h"

/* Bits in a word */
#define w 32
/* Rotate left */
#define ROTL(x, n) ((x >> n) | (x >> (w - n)))
/* Rotate right */
#define ROTR(x, n) ((x >> n) | (x << (w - n)))
/* Right shift */
#define SHR(x, n) (x >> n)

/* Functions */
#define Ch(x, y, z) ((x & y) ^ (~x & z))
#define Maj(x, y, z) ((x & y) ^ (x & z) ^ (y & z))
#define SIGMA0(x) (ROTR(x, 2) ^ ROTR(x, 13) ^ ROTR(x, 22))
#define SIGMA1(x) (ROTR(x, 6) ^ ROTR(x, 11) ^ ROTR(x, 25))
#define sigma0(x) (ROTR(x, 7) ^ ROTR(x, 18) ^ SHR(x, 3))
#define sigma1(x) (ROTR(x, 17) ^ ROTR(x, 19) ^ SHR(x, 10))


char *sha256(char *fn)
{
    /* Message block for SHA-256 is 512 bits (16 * 32-bit words) or 64 bytes */

    char *p, *hash_str, *hs, ch;
    unsigned char *q;
    FILE *fp;
    size_t fs, fs_left, mem_size, read_size, bytes_to_process;
    int le = 0;
    uint32_t test;
    unsigned char md[32], uch;
    uint32_t *z;
    size_t j;

    /* Variables from the standard */
    uint64_t l;
    uint32_t *M;
    uint32_t W[64];
    uint32_t a, b, c, d, e, f, g, h;
    uint32_t T1, T2;
    uint32_t H0, H1, H2, H3, H4, H5, H6, H7;
    size_t i, t;
    /* Number of 64 blocks in bytes_to_process, not the whole message */
    size_t N;

    uint32_t K[64] = {
        0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b,
        0x59f111f1, 0x923f82a4, 0xab1c5ed5,
        0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74,
        0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
        0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f,
        0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
        0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3,
        0xd5a79147, 0x06ca6351, 0x14292967,
        0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354,
        0x766a0abb, 0x81c2c92e, 0x92722c85,
        0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819,
        0xd6990624, 0xf40e3585, 0x106aa070,
        0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3,
        0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
        0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa,
        0xa4506ceb, 0xbef9a3f7, 0xc67178f2
    };

    if (filesize(fn, &fs))
        return NULL;

    if (mof(fs, 8))
        return NULL;

    /* Message length in bits */
    l = fs * 8;

    /* Initial hash value */
    H0 = 0x6a09e667;
    H1 = 0xbb67ae85;
    H2 = 0x3c6ef372;
    H3 = 0xa54ff53a;
    H4 = 0x510e527f;
    H5 = 0x9b05688c;
    H6 = 0x1f83d9ab;
    H7 = 0x5be0cd19;

    /* Set the memory size */
    mem_size = min(BUFSIZ, fs);
    /* Check for possible overflow in the next two steps */
    if (aof(mem_size, 64 * 2))
        return NULL;
    /* Make a multiple of 64 */
    if (mem_size % 64)
        mem_size = (mem_size / 64 + 1) * 64;
    /* Add 64 bytes for padding */
    mem_size += 64;

    /* Open file for reading in binary mode */
    if ((fp = fopen(fn, "rb")) == NULL)
        return NULL;

    /* Allocate memory */
    if ((p = malloc(mem_size)) == NULL) {
        fclose(fp);
        return NULL;
    }

    M = (uint32_t *) p;

    /* Test if little endian */
    test = 0x01020304;
    q = (unsigned char *) &test;
    if (*q == 0x04)
        le = 1;

    fs_left = fs;

    /* Always run at least once (for empty files) */
    do {
        read_size = min(mem_size - 64, fs_left);

        if (fread(p, 1, read_size, fp) != read_size) {
            fclose(fp);
            free(p);
            return NULL;
        }

        fs_left -= read_size;

        if (!fs_left) {
            /* Padding the message */

            /*
             * Make the padded message a multiple of 64 bytes after adding 1
             * byte for the 0x80 byte, and 8 bytes for the total message size
             * in bits (expressed as an 8 byte integer).
             */
            if ((read_size + 1 + 8) % 64)
                bytes_to_process = ((read_size + 1 + 8) / 64 + 1) * 64;
            else
                bytes_to_process = read_size + 1 + 8;   /* Already a multiple */

            /* Zero out end of memmory */
            memset(p + read_size, '\0', bytes_to_process - read_size);

            /* Add the 1 to the end of the message */
            *(p + read_size) = (unsigned char) 0x80;

            /* Write the original message size in bits as an 8 byte integer */
            *((uint64_t *) (p + bytes_to_process - 8)) = l;

            /* Convert size to big endian */
            if (le) {
                j = bytes_to_process - 8;
                ch = p[j];
                p[j] = p[j + 7];
                p[j + 7] = ch;
                ch = p[j + 1];
                p[j + 1] = p[j + 6];
                p[j + 6] = ch;
                ch = p[j + 2];
                p[j + 2] = p[j + 5];
                p[j + 5] = ch;
                ch = p[j + 3];
                p[j + 3] = p[j + 4];
                p[j + 4] = ch;
            }
        } else {
            bytes_to_process = read_size;
        }

        /* Convert message to little endian */
        if (le)
            for (j = 0; j < bytes_to_process; j += 4) {
                ch = p[j];
                p[j] = p[j + 3];
                p[j + 3] = ch;
                ch = p[j + 1];
                p[j + 1] = p[j + 2];
                p[j + 2] = ch;
            }

        N = bytes_to_process / 64;

        /* For each 64 byte block in this read chunk */
        for (i = 0; i < N; ++i) {
            /* Prepare message schedule */
            for (t = 0; t <= 15; ++t)
                W[t] = *(M + i * 16 + t);
            for (t = 16; t <= 63; ++t)
                W[t] =
                    sigma1(W[t - 2]) + W[t - 7] + sigma0(W[t - 15]) + W[t -
                                                                        16];

            /* Initialise working variables */
            a = H0;
            b = H1;
            c = H2;
            d = H3;
            e = H4;
            f = H5;
            g = H6;
            h = H7;

            for (t = 0; t <= 63; ++t) {
                T1 = h + SIGMA1(e) + Ch(e, f, g) + K[t] + W[t];
                T2 = SIGMA0(a) + Maj(a, b, c);
                h = g;
                g = f;
                f = e;
                e = d + T1;
                d = c;
                c = b;
                b = a;
                a = T1 + T2;
            }

            /* Compute intermediate hash value */
            H0 += a;
            H1 += b;
            H2 += c;
            H3 += d;
            H4 += e;
            H5 += f;
            H6 += g;
            H7 += h;

        }
    } while (fs_left);

    free(p);

    if (fclose(fp))
        return NULL;

    /* Message digest */
    z = (uint32_t *) md;

    *z++ = H0;
    *z++ = H1;
    *z++ = H2;
    *z++ = H3;
    *z++ = H4;
    *z++ = H5;
    *z++ = H6;
    *z++ = H7;

    /* Convert message digest to big endian */
    if (le)
        for (j = 0; j < 32; j += 4) {
            uch = md[j];
            md[j] = md[j + 3];
            md[j + 3] = uch;
            uch = md[j + 1];
            md[j + 1] = md[j + 2];
            md[j + 2] = uch;
        }

    /* Dynamically allocate hash string */
    if ((hash_str = malloc(68)) == NULL)
        return NULL;

    /* Create a copy so can increment */
    hs = hash_str;

    /* Convert to hex */
    for (j = 0; j < 32; ++j) {
        uch = md[j];
        *hs++ = uch / 16 >= 10 ? 'a' + uch / 16 - 10 : '0' + uch / 16;
        *hs++ = uch % 16 >= 10 ? 'a' + uch % 16 - 10 : '0' + uch % 16;
    }
    /* Terminate string */
    *hs = '\0';

    return hash_str;
}
