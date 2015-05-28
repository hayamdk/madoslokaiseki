#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stdlib.h>

void write_to_file(FILE *fp, FILE *fp_out, uint32_t offset, uint32_t size)
{
	uint32_t remain, copysize;
	unsigned char buf[1024];
	long pos = ftell(fp);
	fseek(fp, offset, SEEK_SET);
	remain = size;
	while (remain > 0) {
		copysize = remain;
		if (copysize > 1024) {
			copysize = 1024;
		}
		fread(buf, 1, copysize, fp);
		fwrite(buf, 1, copysize, fp_out);
		remain -= copysize;
	}
	fseek(fp, pos, SEEK_SET);
}

const char *change_ext(const char *fname_in, const char *ext)
{
	int fname_len, ext_len;
	char *fname_out, *p;

	fname_len = strlen(fname_in);
	ext_len = strlen(ext);
	fname_out = (char*)malloc(fname_len+ext_len+1);
	strcpy(fname_out, fname_in);
	for (p = &fname_out[fname_len]; *p != '.' && *p != '\\' && p >= fname_out; p--) {
	}
	strcpy(p, ext);

	return fname_out;
}

int main(int argc, const char *argv[])
{
	const char *fname_bin, *fname_add;
	const char *prefix, *suffix;
	char fname[1024];
	char buf[512];
	FILE *fp_bin, *fp_add, *fp_out;

	uint32_t offset, offset_next;
	int ret, count;

	if (argc < 2 || argc > 4) {
		fprintf(stderr, "usage:\nbinsplit.exe [binfile] [prefix] [suffix]\n");
		return 0;
	}

	fname_bin = argv[1];
	if (argc == 2) {
		prefix = "file";
		suffix = "bin";
	} else if (argc == 3) {
		prefix = argv[2];
		suffix = "bin";
	}else {
		prefix = argv[2];
		suffix = argv[3];
	}

	//fname_bin = "H:\\slotmadomagi\\main.6.com.universal777.slotmadoka.obb\\ogg.bin";
	//fname_bin = "H:\\slotmadomagi\\patch.6.com.universal777.slotmadoka.obb\\cri.bin";
	fname_add = change_ext(fname_bin, "_add.bin");
	//prefix = "sound";
	//suffix = "ogg";

	fp_bin = fopen(fname_bin, "rb");
	if (!fp_bin) {
		fprintf(stderr, "open failed!!: %s\n", fname_bin);
		return 1;
	}

	fp_add = fopen(fname_add, "rb");
	if (!fp_add) {
		fprintf(stderr, "open failed!!: %s\n", fname_add);
		fclose(fp_bin);
		return 1;
	}

	count = 0;
	fread(&offset, 4, 1, fp_add);
	while (1) {
		ret = fread(&offset_next, 4, 1, fp_add);
		if (ret == 0) {
			break;
		}

		fseek(fp_bin, offset, SEEK_SET);
		fread(buf, 1, 4, fp_bin);

		/* CRIDムービーのときのみファイル名をヘッダから取る */
		if (strncmp(buf, "CRID", 4) == 0) {
			fseek(fp_bin, offset, SEEK_SET);
			fread(buf, 1, 512, fp_bin);
			if (strncmp(&buf[0x20], "@UTF", 4) == 0) {
				if (strncmp(&buf[0xBD], "@ALP", 4) == 0) {
					strcpy(fname, &buf[0x127]);
				}
				else {
					strcpy(fname, &buf[0x107]);
				}
			} else {
				sprintf(fname, "%s_%05d.%s", prefix, count, suffix);
			}
		} else {
			sprintf(fname, "%s_%05d.%s", prefix, count, suffix);
		}

		fp_out = fopen(fname, "wb");
		if (!fp_out) {
			fprintf(stderr, "open failed!!: %s\n", fname);
			continue;
		}

		printf("%s: %d bytes\n", fname, offset_next - offset);

		write_to_file(fp_bin, fp_out, offset, offset_next - offset);
		fclose(fp_out);

		offset = offset_next;
		count++;
	}
}