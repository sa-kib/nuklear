/* Based on x11_rawfb/main.c */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <limits.h>
#include <math.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <linux/fb.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>
#include <time.h>

#define NK_INCLUDE_FIXED_TYPES
#define NK_INCLUDE_STANDARD_IO
#define NK_INCLUDE_STANDARD_VARARGS
#define NK_INCLUDE_DEFAULT_ALLOCATOR
#define NK_IMPLEMENTATION
#define NK_RAWFB_IMPLEMENTATION
#define NK_INCLUDE_FONT_BAKING
#define NK_INCLUDE_SOFTWARE_FONT
#define NK_RAWFB_INCLUDE_STB_IMAGE
#define DEFAULT_FONT_FILEPATH "/usr/etc/fonts/050-DroidSansFallback.ttf"

#include "../../nuklear.h"
#include "nuklear_rawfb.h"
#include "icon.h"

static int running = nk_true;

void usage(const char *exe)
{
	fprintf(stderr,
		"Usage:\n"
		"\t%s <device>\n",
		exe);
}

static void stop_handler(int signum)
{
	running = nk_false;
}

static inline void sleep_for(long t)
{
    struct timespec req;
    const time_t sec = (int)(t/1000);
    const long ms = t - (sec * 1000);
    req.tv_sec = sec;
    req.tv_nsec = ms * 1000000L;
    while(-1 == nanosleep(&req, &req));
}

typedef struct {int x,y,z;} point_t;

/* Yawing */
static inline point_t rotate_y(point_t p, float angle) {
        float cs = nk_cos(angle), sn = nk_sin(angle);
        return (point_t){.x = cs * p.x + sn * p.z, .y = p.y, .z = cs * p.z - sn * p.x};
}

/* Rolling */
static inline point_t rotate_z(point_t p, float angle) {
        float cs = nk_cos(angle), sn = nk_sin(angle);
        return (point_t){.x = cs * p.x - sn * p.y, .y = sn * p.x + cs * p.y, .z = p.z};
}

static void rotate(struct rawfb_image *dstfb, struct rawfb_image *srcfb,  float angle) {
	unsigned char *dst = dstfb->pixels, *src = srcfb->pixels;
	int width = srcfb->w, height = srcfb->h, pitch = srcfb->pitch;

	NK_ASSERT(pitch / width == 1);
        bzero(dst, pitch * height);

        for (int i = 0; i < height; i++) {
                for (int j = 0; j < width; j++) {
                        point_t sample = { .x = j - width/2,  .y = height/2 - i , .z = 0}; /* to cartesian coords */
                        point_t r = rotate_y(sample, angle); /* r = A * sample */
                        dst[(height/2 - r.y )*pitch + r.x + width/2] += src[i*pitch + j]; /* to screen coords projection with blending */
                }
        }
}

int main(int argc, char *argv[])
{
	int fd;
	void *vaddr;
	struct fb_var_screeninfo var;
	struct fb_fix_screeninfo fix;
	char *devpath = "/dev/fb0";
	struct rawfb_context *rawfb;
	unsigned char *tex_scratch;

	if (argc > 1 && !strcmp(argv[1], "-h")) {
		usage(argv[0]);
		return 0;
	}

	tex_scratch = malloc(1024 * 32768); /* enough for Chinese typefaces */

	if (argc > 1)
		devpath = argv[1];

	fd = open(devpath, O_RDWR);
	if (fd < 0) {
		fprintf(stderr, "failed to open %s: %s\n", devpath, strerror(errno));
		return -ENODEV;
	}

	if (ioctl(fd, FBIOGET_VSCREENINFO, &var) < 0 || ioctl(fd, FBIOGET_FSCREENINFO, &fix) < 0) {
		fprintf(stderr, "get screen info failed\n");
		return -EFAULT;
	}

	printf("FB settings:\n");
	printf("\tID: %s\n", fix.id);
	printf("\tresolution: %dx%d@%u\n", var.xres, var.yres, var.bits_per_pixel);
	printf("\tstride: %u bytes\n", fix.line_length);
	printf("\tsize: %u bytes\n", fix.smem_len);

	vaddr = mmap(NULL, fix.smem_len, PROT_WRITE, MAP_SHARED, fd, 0);
	if (vaddr == MAP_FAILED) {
		fprintf(stderr, "failed to map framebuffer: %s\n", strerror(errno));
		return -EBADF;
	}

	rawfb = nk_rawfb_init(vaddr, tex_scratch, var.xres, var.yres, fix.line_length);
	if (!rawfb) {
		fprintf(stderr, "failed to init rawfb\n");
		return -ENODEV;
	}

	struct nk_image icon = nk_rawfb_load_image_from_memory(icon_data, icon_data_len);
	struct nk_context *ctx = &rawfb->ctx;
	unsigned char *img = malloc(icon.w * icon.h);
	struct nk_image rot = nk_stbi_image_to_fbimage(img, icon.w, icon.h, 1);
	const float M_2PI  = M_PI * 2.f, STEP = 0.263f /* 15° */ ;
	float angle = 0;
	const char utf8_msgs[7][18] = {
		{ /* UBNT Demo */
			0x55, 0x42, 0x4e, 0x54, 0x20, 0x44, 0x65, 0x6d, 0x6f
		},
		{ /* русский */
			0xd1, 0x80, 0xd1, 0x83, 0xd1, 0x81, 0xd1, 0x81, 0xd0,
			0xba, 0xd0, 0xb8, 0xd0, 0xb9
		},
		{ /* 中文 */
			0xe4, 0xb8, 0xad, 0xe6, 0x96, 0x87
		},
		{ /* 汉语 */
			0xe6, 0xb1, 0x89, 0xe8, 0xaf, 0xad
		},
		{ /* 漢語 */
			0xe6, 0xbc, 0xa2, 0xe8, 0xaa, 0x9e
		},
		{ /* 日本語 */
			0xe6, 0x97, 0xa5, 0xe6, 0x9c, 0xac, 0xe8, 0xaa, 0x9e
		},
		{ /* にほんご */
			0xe3, 0x81, 0xab, 0xe3, 0x81, 0xbb, 0xe3, 0x82, 0x93,
			0xe3, 0x81, 0x94
		},
	};

	signal(SIGINT, stop_handler);

	for (unsigned long loop = 0; running; loop++) {
		nk_flags window_flags = NK_WINDOW_BORDER | NK_WINDOW_NO_SCROLLBAR | NK_WINDOW_NO_INPUT;
		nk_size prog = loop % 100;
		if (nk_begin(ctx, "UBNT Demo",
			     nk_rect(5, 5, var.xres - 5, var.yres - 10),
			     window_flags)) {
			nk_layout_row_begin(ctx, NK_STATIC, 30, 2);
			nk_layout_row_push(ctx, 30);
			angle = angle + STEP > M_2PI ? angle + STEP - M_2PI : angle + STEP;
			rotate(rot.handle.ptr, icon.handle.ptr, angle);
			nk_image(ctx, rot);
			nk_layout_row_push(ctx, 110);
			nk_label(ctx, utf8_msgs[(loop/100)%7] , NK_TEXT_CENTERED);
			nk_layout_row_end(ctx);
			nk_layout_row_dynamic(ctx, 15, 1);
			nk_progress(ctx, &prog, 100, NK_FIXED);
		}
		nk_end(ctx);
		nk_rawfb_render(rawfb, nk_rgb(0,0,0), 1);
		sleep_for(90);
	}

	free(tex_scratch);
	nk_rawfb_clear(rawfb, nk_rgb(0,0,0));
	nk_rawfb_shutdown(rawfb);
	munmap(vaddr, fix.smem_len);
	close(fd);
	return 0;
}
