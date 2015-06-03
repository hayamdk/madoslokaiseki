#define _CRT_SECURE_NO_WARNINGS

#pragma comment(lib, "zdll.lib")

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <float.h>
#include <math.h>

#include "zconf.h"
#include "zlib.h"

#define inline __inline

#include "rgb32bmp.h"
#include "crc32.h"

/*ホスト→ビッグエンディアン*/
static inline uint32_t htobe32(uint32_t host)
{
	uint32_t be;
	((uint8_t*)&be)[0] = (uint8_t)(host >> 24);
	((uint8_t*)&be)[1] = (uint8_t)(host >> 16);
	((uint8_t*)&be)[2] = (uint8_t)(host >> 8);
	((uint8_t*)&be)[3] = (uint8_t)host;
	return be;
}

/*ビッグエンディアン→ホスト*/
static inline uint32_t betoh32(uint32_t be)
{
	uint32_t host;
	host = ((uint32_t)(((uint8_t*)&be)[0]) << 24) +
		((uint32_t)(((uint8_t*)&be)[1]) << 16) +
		((uint32_t)(((uint8_t*)&be)[2]) << 8) +
		(uint32_t)(((uint8_t*)&be)[3]);
	return host;
}

/*ホスト→リトルエンディアン*/
static inline uint32_t htole32(uint32_t host)
{
	uint32_t le;
	((uint8_t*)&le)[0] = (uint8_t)host;
	((uint8_t*)&le)[1] = (uint8_t)(host >> 8);
	((uint8_t*)&le)[2] = (uint8_t)(host >> 16);
	((uint8_t*)&le)[3] = (uint8_t)(host >> 24);
	return le;
}

/*リトルエンディアン→ホスト*/
static inline uint32_t letoh32(uint32_t le)
{
	uint32_t host;
	host = ((uint32_t)(((uint8_t*)&le)[3]) << 24) +
		((uint32_t)(((uint8_t*)&le)[2]) << 16) +
		((uint32_t)(((uint8_t*)&le)[1]) << 8) +
		(uint32_t)(((uint8_t*)&le)[0]);
	return host;
}

rgb32bmp *rgb32bmp_create( int width, int height, int alpha_flg )
{
	rgb32bmp *bmp = (rgb32bmp*)malloc(sizeof(rgb32bmp));
	bmp->width = width;
	bmp->height = height;
	bmp->alpha_flg = alpha_flg;
	bmp->buf = (uint8_t*)malloc(width*height*4);
	memset(bmp->buf, 255, width*height*4 );

	return bmp;
}

static inline int upper4(int a)
{
	return ((a+3)/4)*4;
}

void rgb32bmp_delete(rgb32bmp *bmp)
{
	free(bmp->buf);
	free(bmp);
}

int rgb32bmp_output_bmp(rgb32bmp *bmp, const char *fname)
{
	FILE *fp;
	int filesize, bufsize, linesize;
	int x, y;
	uint8_t *buf, *line, r, g, b;
	uint8_t header[] = {
		0x42, 0x4D, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x36, 0x00, 0x00, 0x00, 0x28, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x18, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x00, 0x00, 0x00, 0x00, 0x00, 0x00
	};

	linesize = upper4(bmp->width * 3);
	bufsize = linesize * bmp->height;
	filesize = bufsize + sizeof(header);

	*((uint32_t*)&header[0x02]) = htole32(filesize);
	*((uint32_t*)&header[0x12]) = htole32(bmp->width);
	*((uint32_t*)&header[0x16]) = htole32(bmp->height);
	*((uint32_t*)&header[0x22]) = htole32(bufsize);

	buf = (uint8_t*)malloc(bufsize);
	for (y = 0; y < bmp->height; y++) {
		line = &buf[(bmp->height-y-1) * linesize];
		for (x = 0; x < bmp->width; x++) {
			get_rgb(bmp, x, y, &r, &g, &b);
			line[x*3 + 0] = b;
			line[x*3 + 1] = g;
			line[x*3 + 2] = r;
		}
	}

	fp = fopen(fname, "wb");
	if (!fp) {
		return 0;
	}
	fwrite(header, 1, sizeof(header), fp);
	fwrite(buf, 1, bufsize, fp);
	fclose(fp);
	return 1;
}

static void write_png_chunk2(FILE *fp, const uint8_t *chunk_data1, const uint8_t *chunk_data2)
{
	int chunk_size = betoh32(*(uint32_t*)chunk_data1);
	uint32_t crc32 = update_crc32(0xffffffffL, &chunk_data1[4], 4);
	crc32 = update_crc32(crc32, chunk_data2, chunk_size) ^ 0xffffffffL;
	crc32 = htobe32(crc32);
	fwrite(chunk_data1, 1, 8, fp);
	fwrite(chunk_data2, 1, chunk_size, fp);
	fwrite(&crc32, 4, 1, fp);
}

static void write_png_chunk(FILE *fp, const uint8_t *chunk_data)
{
	int chunk_size = betoh32(*(uint32_t*)chunk_data);
	uint32_t crc32 = calc_crc32(&chunk_data[4], chunk_size + 4);
	crc32 = htobe32(crc32);
	fwrite(chunk_data, 1, chunk_size + 8, fp);
	fwrite(&crc32, 4, 1, fp);
}

static inline void png_average_predictor(rgb32bmp *bmp, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint8_t ur=0, ug=0, ub=0, ua=0, lr=0, lg=0, lb=0, la=0;

	if (y > 0) {
		get_rgba(bmp, x, y-1, &ur, &ug, &ub, &ua);
	}
	if (x > 0) {
		get_rgba(bmp, x-1, y, &lr, &lg, &lb, &la);
	}
	*r = (int8_t)(((int)ur + lr) / 2);
	*g = (int8_t)(((int)ug + lg) / 2);
	*b = (int8_t)(((int)ub + lb) / 2);
	*a = (int8_t)(((int)ua + la) / 2);
}

static inline uint8_t paeth_predictor(uint8_t left, uint8_t above, uint8_t upperleft)
{
	int pa, pb, pc;
	pa = abs((int)above - upperleft);
	pb = abs((int)left - upperleft);
	pc = abs((int)left + above - 2*upperleft);

	if (pa <= pb && pa <= pc) {
		return left;
	}else if (pb <= pc) {
		return above;
	}
	return upperleft;
}

static inline void png_paeth_predictor(rgb32bmp *bmp, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint8_t lr=0, lg=0, lb=0, la=0, ar=0, ag=0, ab=0, aa=0, ulr=0, ulg=0, ulb=0, ula=0;
	if (x > 0) {
		get_rgba(bmp, x-1, y, &lr, &lg, &lb, &la);
		if (y > 0) {
			get_rgba(bmp, x-1, y-1, &ulr, &ulg, &ulb, &ula);
		}
	}
	if (y > 0) {
		get_rgba(bmp, x, y-1, &ar, &ag, &ab, &aa);
	}

	*r = paeth_predictor(lr, ar, ulr);
	*g = paeth_predictor(lg, ag, ulg);
	*b = paeth_predictor(lb, ab, ulb);
	if (bmp->alpha_flg) {
		*a = paeth_predictor(la, aa, ula);
	}
}

static void png_line_filter(rgb32bmp *bmp, uint8_t *line, int y, int type)
{
	int x, bpp = 3;
	uint8_t r, g, b, a, r1=0, g1=0, b1=0, a1=0;

	line[0] = (uint8_t)type;
	line++;

	if (bmp->alpha_flg) {
		bpp = 4;
	}
	for (x = 0; x < bmp->width; x++) {
		get_rgba(bmp, x, y, &r, &g, &b, &a);
		if (type == 0) {		/* Filter type 0: None */
			line[x*bpp + 0] = r;
			line[x*bpp + 1] = g;
			line[x*bpp + 2] = b;
			if (bmp->alpha_flg) {
				line[x*bpp + 3] = a;
			}
		} else if (type == 1) { /* Filter type 1: Sub */
			line[x*bpp + 0] = r - r1;
			line[x*bpp + 1] = g - g1;
			line[x*bpp + 2] = b - b1;
			if (bmp->alpha_flg) {
				line[x*bpp + 3] = a - a1;
				a1 = a;
			}
			r1 = r; g1 = g; b1 = b;
		} else if (type == 2) { /* Filter type 2: Up */
			if (y > 0) {
				get_rgba(bmp, x, y-1, &r1, &g1, &b1, &a1);
			}
			line[x*bpp + 0] = r - r1;
			line[x*bpp + 1] = g - g1;
			line[x*bpp + 2] = b - b1;
			if (bmp->alpha_flg) {
				line[x*bpp + 3] = a - a1;
			}
		} else if (type == 3) { /* Filter type 3: Average */
			png_average_predictor(bmp, x, y, &r1, &g1, &b1, &a1);
			line[x*bpp + 0] = r - r1;
			line[x*bpp + 1] = g - g1;
			line[x*bpp + 2] = b - b1;
			if (bmp->alpha_flg) {
				line[x*bpp + 3] = a - a1;
			}
		} else {				/* Filter type 4: Paeth */
			png_paeth_predictor(bmp, x, y, &r1, &g1, &b1, &a1);
			line[x*bpp + 0] = r - r1;
			line[x*bpp + 1] = g - g1;
			line[x*bpp + 2] = b - b1;
			if (bmp->alpha_flg) {
				line[x*bpp + 3] = a - a1;
			}
		}
	}
}

static inline double calc_line_entropy(uint8_t *line, int linesize)
{
	int i;
	int count[256];
	double entropy = 0.0;

	if (linesize <= 0) {
		return 0.0;
	}

	for (i = 0; i < 256; i++) {
		count[i] = 0;
	}
	for (i = 0; i < linesize; i++) {
		count[line[i]]++;
	}
	for (i = 0; i < 256; i++) {
		if (count[i] > 0) {
			entropy += (double)count[i] * log((double)linesize / count[i]);
		}
	}
	entropy /= log(2);
	return entropy;
}

int rgb32bmp_output_png(rgb32bmp *bmp, const char *fname)
{
	FILE *fp;
	int i;
	int linesize, allocsize, y, ret, bpp, type;
	uLongf datasize;
	uint8_t *buf, *png_IDAT2, *line;
	double em, e;

	uint8_t png_sig[] = {
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
	};
	uint8_t png_IHDR[] = {
		0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x02, 0x00, 0x00, 0x00
	};
	uint8_t png_IEND[] = {
		0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D'
	};
	uint8_t png_IDAT1[] = {
		0x00, 0x00, 0x00, 0x00, 'I', 'D', 'A', 'T'
	};

	if (bmp->alpha_flg) {
		png_IHDR[0x11] = 0x06;
	}

	*(uint32_t*)&png_IHDR[0x08] = htobe32(bmp->width);
	*(uint32_t*)&png_IHDR[0x0c] = htobe32(bmp->height);

	if (bmp->alpha_flg) {
		bpp = 4;
	} else {
		bpp = 3;
	}

	linesize = bmp->width * bpp + 1;
	allocsize = (int)((double)(linesize*bmp->height)*1.01) + 16;
	buf = (uint8_t*)malloc(linesize*bmp->height);
	png_IDAT2 = (uint8_t*)malloc(allocsize);

	for (y = 0; y < bmp->height; y++) {
		line = &buf[y * linesize];
		em = DBL_MAX;
		type = 0;
		/* 5種類のフィルタを試して一番エントロピーが小さいものを選ぶ */
		for (i = 0; i <= 4; i++) {
			png_line_filter(bmp, line, y, i);
			e = calc_line_entropy(line, linesize);
			if (em > e) {
				type = i;
				em = e;
			}
		}
		if (type != 4) {
			png_line_filter(bmp, line, y, type);
		}
	}

	datasize = allocsize;
	ret = compress2(png_IDAT2, &datasize, buf, linesize*bmp->height, 9);
	if (ret != 0) {
		free(buf);
		free(png_IDAT2);
		return 0;
	}

	*((uint32_t*)png_IDAT1) = htobe32((uint32_t)datasize);

	fp = fopen(fname, "wb");
	if (!fp) {
		free(buf);
		free(png_IDAT2);
		return 0;
	}

	fwrite(png_sig, 1, sizeof(png_sig), fp);
	write_png_chunk(fp, png_IHDR);
	write_png_chunk2(fp, png_IDAT1, png_IDAT2);
	write_png_chunk(fp, png_IEND);
	fclose(fp);

	free(buf);
	free(png_IDAT2);

	return 1;
}