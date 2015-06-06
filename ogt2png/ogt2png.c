#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define inline __inline

#include "crc32.h"
#include "rgb32bmp.h"

#include "lz4.h"

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

static void write_png_chunk(FILE *fp, const uint8_t *chunk_data)
{
	int chunk_size = betoh32( *(uint32_t*)chunk_data );
	uint32_t crc = calc_crc32(&chunk_data[4], chunk_size+4);
	crc = htobe32(crc);
	fwrite(chunk_data, 1, chunk_size+8, fp);
	fwrite(&crc, 4, 1, fp);
}

int save_fullcolor_png(FILE *fp, const char *fname_out, uint32_t width, uint32_t height)
{
	FILE *fp_out;
	uint32_t datasize;

	const uint8_t png_sig[] = {
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
	};
	uint8_t png_IHDR[] = {
		0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x06, 0x00, 0x00, 0x00
	};
	const uint8_t png_IEND[] = {
		0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D'
	};
	uint8_t *png_IDAT;

	fp_out = fopen(fname_out, "wb");
	if (!fp_out) {
		fprintf(stderr, "open failed!!: %s\n", fname_out);
		return 0;
	}

	*(uint32_t*)&png_IHDR[0x08] = htobe32(width);
	*(uint32_t*)&png_IHDR[0x0c] = htobe32(height);

	fread(&datasize, 4, 1, fp);
	png_IDAT = (uint8_t*)malloc(datasize + 8);
	fread(&png_IDAT[8], 1, datasize, fp);
	*((uint32_t*)png_IDAT) = htobe32(datasize);
	memcpy(&png_IDAT[4], "IDAT", 4);

	fwrite(png_sig, 1, sizeof(png_sig), fp_out);
	write_png_chunk(fp_out, png_IHDR);
	write_png_chunk(fp_out, png_IDAT);
	write_png_chunk(fp_out, png_IEND);

	free(png_IDAT);
	fclose(fp_out);

	return 1;
}

int save_indexcolor_png(FILE *fp, const char *fname_out, uint32_t width, uint32_t height)
{
	FILE *fp_out;
	uint32_t datasize;
	int i;

	const uint8_t png_sig[] = {
		0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A
	};
	uint8_t png_IHDR[] = {
		0x00, 0x00, 0x00, 0x0D, 'I', 'H', 'D', 'R', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
		0x08, 0x03, 0x00, 0x00, 0x00
	};
	const uint8_t png_IEND[] = {
		0x00, 0x00, 0x00, 0x00, 'I', 'E', 'N', 'D'
	};
	uint8_t *png_PLTE, *png_tRNS, *png_IDAT;

	uint8_t colortable[0x400];

	fp_out = fopen(fname_out, "wb");
	if (!fp_out) {
		fprintf(stderr, "open failed!!: %s\n", fname_out);
		return 0;
	}

	*(uint32_t*)&png_IHDR[0x08] = htobe32(width);
	*(uint32_t*)&png_IHDR[0x0c] = htobe32(height);

	fread(&colortable, 1, 0x400, fp);
	fread(&datasize, 4, 1, fp);

	png_IDAT = (uint8_t*)malloc(datasize + 8);
	fread(&png_IDAT[8], 1, datasize, fp);
	*((uint32_t*)png_IDAT) = htobe32(datasize);
	memcpy(&png_IDAT[4], "IDAT", 4);

	png_PLTE = (uint8_t*)malloc(3*256 + 8);
	*((uint32_t*)png_PLTE) = htobe32(3*256);
	memcpy(&png_PLTE[4], "PLTE", 4);
	for (i = 0; i < 256; i++) {
		png_PLTE[8 + i*3] = colortable[i*4];
		png_PLTE[8 + i*3 + 1] = colortable[i*4+1];
		png_PLTE[8 + i*3 + 2] = colortable[i*4+2];
	}

	png_tRNS = (uint8_t*)malloc(256 + 8);
	*((uint32_t*)png_tRNS) = htobe32(256);
	memcpy(&png_tRNS[4], "tRNS", 4);
	for (i = 0; i < 256; i++) {
		png_tRNS[8+i] = colortable[i*4+3];
	}

	fwrite(png_sig, 1, sizeof(png_sig), fp_out);
	write_png_chunk(fp_out, png_IHDR);
	write_png_chunk(fp_out, png_PLTE);
	write_png_chunk(fp_out, png_tRNS);
	write_png_chunk(fp_out, png_IDAT);
	write_png_chunk(fp_out, png_IEND);

	free(png_tRNS);
	free(png_PLTE);
	free(png_IDAT);
	fclose(fp_out);

	return 1;
}

static const int etc1_mod_table[] = {
	/* 0 */ 2, 8, -2, -8,
	/* 1 */ 5, 17, -5, -17,
	/* 2 */ 9, 29, -9, -29,
	/* 3 */ 13, 42, -13, -42,
	/* 4 */ 18, 60, -18, -60,
	/* 5 */ 24, 80, -24, -80,
	/* 6 */ 33, 106, -33, -106,
	/* 7 */ 47, 183, -47, -183
};

static inline int convert4to8(int a)
{
	return a*16 + a;
}

static inline int convert5to8(int a)
{
	return a*8 + a/4;
}

rgb32bmp *decode_etc1(uint8_t *etc1_buf, int width, int height)
{
	int i, j, k, x1, x2, y1, y2, x_base, y_base;
	int bw, bh, flip_bit, diff_bit, tbase1, tbase2, idx1, idx2, r1, r2, g1, g2, b1, b2, diff1, diff2;
	uint8_t *block;
	rgb32bmp *bmp = rgb32bmp_create(width, height, 0);

	bw = width / 4;
	bh = height / 4;

	for (j = 0; j<bh; j++) {
		for (i = 0; i<bw; i++) {
			block = &etc1_buf[ (j*bw + i) * 8 ];
			x_base = i * 4;
			y_base = j * 4;
			flip_bit = block[3] & 0x01;
			diff_bit = (block[3]>>1) & 0x01;

			tbase1 = block[3]>>5;
			tbase2 = (block[3]>>2) & 0x07;

			/* decide base colors */
			if(diff_bit) {
				r1 = block[0] >> 3;
				r2 = r1 + ((block[0]+4)&0x07) - 4;
				g1 = block[1] >> 3;
				g2 = g1 + ((block[1]+4)&0x07) - 4;
				b1 = block[2] >> 3;
				b2 = b1 + ((block[2]+4)&0x07) - 4;
				r1 = convert5to8(r1);
				r2 = convert5to8(r2);
				g1 = convert5to8(g1);
				g2 = convert5to8(g2);
				b1 = convert5to8(b1);
				b2 = convert5to8(b2);
			} else {
				r1 = convert4to8(block[0] >> 4);
				r2 = convert4to8(block[0] & 0x0f);
				g1 = convert4to8(block[1] >> 4);
				g2 = convert4to8(block[1] & 0x0f);
				b1 = convert4to8(block[2] >> 4);
				b2 = convert4to8(block[2] & 0x0f);
			}

			/* decode subblocks */
			for (k = 0; k < 8; k++) {
				if (!flip_bit) {
					x1 = x_base + k/4;
					x2 = x1 + 2;
					y2 = y1 = y_base + k%4;
					idx1 = (block[5]>>k) % 2 * 2 + (block[7]>>k) % 2;
					idx2 = (block[4]>>k) % 2 * 2 + (block[6]>>k) % 2;
				} else {
					x1 = x2 = x_base + k/2;
					y1 = y_base + k%2;
					y2 = y1 + 2;
					idx1 = (block[5-k/4]>>(k%2+(k&0x02)*2)) % 2 * 2 + (block[7-k/4]>>(k%2+(k&0x02)*2)) % 2;
					idx2 = (block[5-k/4]>>(2+k%2+(k&0x02)*2)) % 2 * 2 + (block[7-k/4]>>(2+k%2+(k&0x02)*2)) % 2;
				}

				diff1 = etc1_mod_table[tbase1*4 + idx1];
				diff2 = etc1_mod_table[tbase2*4 + idx2];
				set_rgb_clipping(bmp, x1, y1, r1 + diff1, g1 + diff1, b1 + diff1);
				set_rgb_clipping(bmp, x2, y2, r2 + diff2, g2 + diff2, b2 + diff2);
			}
		}
	}
	return bmp;
}

/* ETC1をデコードした画像は上半分がフルカラー、下半分がアルファになっているのでこの関数で統合する */
rgb32bmp *combile_alpha(rgb32bmp *bmp)
{
	int x, y;
	uint8_t r, g, b, a, r2, g2, b2;
	rgb32bmp *bmp2 = rgb32bmp_create(bmp->width, bmp->height/2, 1);

	for (y = 0; y < bmp2->height; y++) {
		for (x = 0; x < bmp2->width; x++) {
			get_rgb(bmp, x, y, &r, &g, &b);
			get_rgb(bmp, x, y + bmp2->height, &r2, &g2, &b2);
			a = r2;
			if (a > g2) { a = g2; }
			if (a > b2) { a = b2; }
			set_rgba(bmp2, x, y, r, g, b, a);
		}
	}

	rgb32bmp_delete(bmp);
	return bmp2;
}

int save_etc1_to_png(FILE *fp, const char *fname_out, uint32_t allsize, uint32_t width, uint32_t height)
{
	int ret;
	uint32_t var;
	fread(&var, 4, 1, fp);

	uint8_t *data = malloc(allsize + 10);
	uint8_t *buf = malloc(width*height / 2);
	memset(data, 0x00, allsize + 10); /* 少し多めに0で埋めておくとノイズが残りにくい（謎） */
	memset(buf, 0x00, width*height / 2);
	fread(data, 1, allsize-4, fp); /* 本来の画像データサイズはallsize-4 */

	/* ここでLZ4_decompress_safe()は負の値を返すので本当は解凍に成功していない（謎） */
	ret = LZ4_decompress_safe((char*)data, (char*)buf, allsize+10, width*height / 2);

	/*int i = 0x34400;
	uint8_t tmp[] = { 0x00, 0x00, 0x00, 0x02, 0xff, 0xff, 0xff, 0xff };
	for (; i < 0x40000; i += 8) {
		memcpy(&buf[i], tmp, 8);
	}*/

	/*uint8_t *data2 = malloc(16*1024*1024);
	int datasize = 0;
	datasize = LZ4_compress_default(buf, data2, width*height/2, 1024*1024*16);*/

	/*
	FILE *fp2 = fopen("test.bin", "wb");
	fwrite(data, 1, allsize-4, fp2);
	fclose(fp2);

	fp2 = fopen("test2.bin", "wb");
	fwrite(buf, 1, width*height / 2, fp2);
	fclose(fp2);

	fp2 = fopen("test3.bin", "wb");
	fwrite(data2, 1, datasize, fp2);
	fclose(fp2);
	*/

	rgb32bmp *bmp = decode_etc1(buf, width, height);
	//rgb32bmp_output_bmp(bmp, "test.bmp");
	bmp = combile_alpha(bmp);
	ret = rgb32bmp_output_png(bmp, fname_out);
	rgb32bmp_delete(bmp);

	free(buf);
	free(data);

	return ret;
}

void convert_ogt(const char *fname_in, const char *fname_out)
{
	FILE *fp;
	uint16_t mark, type, height, width;
	uint32_t allsize, dummy;

	fp = fopen(fname_in, "rb");
	if (!fp) {
		fprintf(stderr, "open failed!!: %s\n", fname_in);
		return;
	}

	fread(&mark, 2, 1, fp);
	if (mark != 0x0010) {
		fprintf(stderr, "invalid format!!: %s\n", fname_in);
		fclose(fp);
		return;
	}

	fread(&type, 2, 1, fp);
	fread(&width, 2, 1, fp);
	fread(&height, 2, 1, fp);
	fread(&allsize, 4, 1, fp);
	fread(&dummy, 4, 1, fp);

	if (type == 0x0803) {
		/* Index colored(256colors) RGBA */
		printf("%s: RGBA(256colors)\n", fname_in);
		save_indexcolor_png(fp, fname_out, width, height);
	} else if (type == 0x0800) {
		/* Full color RGBA */
		printf("%s: RGBA fullcolor\n", fname_in);
		save_fullcolor_png(fp, fname_out, width, height);
	} else if (type == 0x0006) {
		printf("%s: Compressed texture(ETC1) format\n", fname_in);
		save_etc1_to_png(fp, fname_out, allsize, width, height);
	} else {
		fprintf(stderr, "unknown format: %s\n", fname_in);
	}

	fclose(fp);
}

const char *change_ext(const char *fname_in, const char *ext)
{
	int fname_len, ext_len;
	char *fname_out, *p;

	fname_len = strlen(fname_in);
	ext_len = strlen(ext);
	fname_out = (char*)malloc(fname_len + ext_len + 1);
	strcpy(fname_out, fname_in);
	for (p = &fname_out[fname_len]; *p != '.' && *p != '\\' && p >= fname_out; p--) {
	}
	strcpy(p, ext);

	return fname_out;
}

int main(int argc, const char *argv[])
{
	int i;
	const char *fname_in, *fname_out;

	if (argc < 2) {
		printf("invalid argument!\nUsage: ogt2png.exe [input file1] [input file2] ...\n");
		return 1;
	}

	for (i = 1; i < argc; i++) {
		fname_in = argv[i];
		fname_out = change_ext(fname_in, ".png");
		convert_ogt(fname_in, fname_out);
		free((char*)fname_out);
	}

	return 0;
}