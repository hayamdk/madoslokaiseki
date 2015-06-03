typedef struct {
	int width;
	int height;
	int alpha_flg;
	uint8_t *buf;
} rgb32bmp;

rgb32bmp *rgb32bmp_create(int width, int height, int alpha_flg);
void rgb32bmp_delete(rgb32bmp *bmp);
int rgb32bmp_output_bmp(rgb32bmp *bmp, const char *fname);
int rgb32bmp_output_png(rgb32bmp *bmp, const char *fname);

static inline uint8_t clipping8bit(int a)
{
	if (a > 255) {
		return 255;
	} else if (a < 0) {
		return 0;
	}
	return (uint8_t)a;
}

static inline void set_rgba(rgb32bmp *bmp, int x, int y, uint8_t r, uint8_t g, uint8_t b, uint8_t a)
{
	uint8_t *p = &( bmp->buf[((bmp->width)*y+x)*4] );
	if (x < 0 || x >= bmp->width || y < 0 || y >= bmp->height) {
		return;
	}
	p[0] = r;
	p[1] = g;
	p[2] = b;
	p[3] = a;
}

static inline void set_rgb(rgb32bmp *bmp, int x, int y, uint8_t r, uint8_t g, uint8_t b)
{
	uint8_t *p = &(bmp->buf[((bmp->width)*y + x) * 4]);

	if (x < 0 || x >= bmp->width || y < 0 || y >= bmp->height) {
		return;
	}
	p[0] = r;
	p[1] = g;
	p[2] = b;
	if (bmp->alpha_flg) {
		p[3] = 255;
	}
}

static inline void set_rgba_clipping(rgb32bmp *bmp, int x, int y, int r, int g, int b, int a)
{
	set_rgba(bmp, x, y, clipping8bit(r), clipping8bit(g), clipping8bit(b), clipping8bit(a));
}

static inline void set_rgb_clipping(rgb32bmp *bmp, int x, int y, int r, int g, int b)
{
	set_rgb(bmp, x, y, clipping8bit(r), clipping8bit(g), clipping8bit(b));
}

static inline void get_rgba(rgb32bmp *bmp, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b, uint8_t *a)
{
	uint8_t *p = &(bmp->buf[((bmp->width)*y + x) * 4]);
	if (x < 0 || x >= bmp->width || y < 0 || y >= bmp->height) {
		return;
	}
	*r = p[0];
	*g = p[1];
	*b = p[2];
	*a = p[3];
}

static inline void get_rgb(rgb32bmp *bmp, int x, int y, uint8_t *r, uint8_t *g, uint8_t *b)
{
	uint8_t *p = &(bmp->buf[((bmp->width)*y + x) * 4]);
	if (x < 0 || x >= bmp->width || y < 0 || y >= bmp->height) {
		return;
	}
	*r = p[0];
	*g = p[1];
	*b = p[2];
}