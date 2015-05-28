#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <stdint.h>

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

int main(int argc, const char *argv[])
{
	FILE *fp, *fp_out;
	const char *obb_fname;
	uint32_t n_files;
	uint32_t offset, size;
	uint8_t fname_len;
	char fname[256];
	int i;

	if (argc != 2) {
		fprintf(stderr, "usage:\nobbextract.exe [obbfile]\n");
		return 0;
	}

	//obb_fname = "Q:\\obb\\com.universal777.slotmadoka\\main.6.com.universal777.slotmadoka.obb";
	//obb_fname = "Q:\\obb\\com.universal777.slotmadoka\\patch.6.com.universal777.slotmadoka.obb";
	obb_fname = argv[1];

	fp = fopen(obb_fname, "rb");
	if (!fp) {
		fprintf(stderr, "open failed!!: %s\n", obb_fname);
		return 1;
	}

	fread(&n_files, 4, 1, fp);
	for (i = 0; i < (int)n_files; i++) {
		fread(&offset,		4, 1, fp);
		fread(&size,		4, 1, fp);
		fread(&fname_len,	1, 1, fp);
		fread(&fname,		1, (int)fname_len, fp);
		fname[(int)fname_len] = '\0';
		fp_out = fopen(fname, "wb");
		if (!fp_out) {
			fprintf(stderr, "open failed!!: %s\n", fname);
			continue;
		}
		printf("open: %s (%d bytes)\n", fname, size);
		write_to_file(fp, fp_out, offset, size);
		fclose(fp_out);
	}
	fclose(fp);
	return 0;
}