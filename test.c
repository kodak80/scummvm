/*
 * Standalone reproducer for the "Drive P: not responding" dialog seen in
 * scummvm.prg when reading P:/MONKEY1/MONKEY.001 on TOS-030 / no TT-RAM.
 *
 * test.tos doesn't reproduce the issue — but it's tiny.  This version pads
 * the binary to match scummvm.prg's ~5.5 MB load image, to test whether
 * binary size / TPA layout alone is the variable.
 *
 * All output goes via the sidecart logger (writes to ROM3) so it lands in
 * the minicom capture and can be copy/pasted.
 *
 * Build: m68k-atari-mintelf-gcc -m68020-60 -O2 -o test.tos test.c
 */

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mint/basepage.h>
#include <mint/osbind.h>

#define MONKEY_PATH "P:/MONKEY1/MONKEY.001"

/* Pad the binary so its load image matches scummvm.prg (~5.5 MB).
 * volatile + used so -O2 can't elide it. */
__attribute__((used)) volatile char pad_bss[5500000];

/* --- sidecart debug output (same trick scummvm uses) ----------------- */

static void sidecart_puts(const char *s) {
	const unsigned long CARTRIDGE_ROM3 = 0xFB0000ul;
	for (; *s; s++)
		(void)(*((volatile unsigned short *)(CARTRIDGE_ROM3 + ((*s & 0xFF) << 1))));
}

static void sidecart_printf(const char *fmt, ...) {
	char buf[256];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(buf, sizeof(buf), fmt, ap);
	va_end(ap);
	sidecart_puts(buf);
}

/* --------------------------------------------------------------------- */

int main(int argc, char **argv) {
	const char *path = (argc > 1) ? argv[1] : MONKEY_PATH;
	FILE *fp;
	unsigned char b1[1];
	unsigned char b4[4];
	unsigned char *bigBuf;
	size_t got;
	int i;

	sidecart_printf("test.tos: starting (path='%s')\n", path);

	/* Touch pad_bss so the linker keeps it. */
	pad_bss[0] = 0;
	pad_bss[sizeof(pad_bss) - 1] = 0;

	if (_base) {
		sidecart_printf("basepage=%p tpa=[%p..%p] text=%p+%lx data=%p+%lx bss=%p+%lx\n",
		                (void *)_base,
		                (void *)_base->p_lowtpa, (void *)_base->p_hitpa,
		                (void *)_base->p_tbase, _base->p_tlen,
		                (void *)_base->p_dbase, _base->p_dlen,
		                (void *)_base->p_bbase, _base->p_blen);
	}
	sidecart_printf("free ST-RAM: %ld bytes\n", (long)Mxalloc(-1L, 0));

	fp = fopen(path, "rb");
	if (!fp) {
		sidecart_printf("fopen('%s') failed\n", path);
		return 1;
	}

	fseek(fp, 0, SEEK_SET);
	fseek(fp, 16, SEEK_SET);

	fread(b1, 1, 1, fp);
	fread(b1, 1, 1, fp);

	fread(b4, 1, 4, fp);
	for (i = 0; i < 82; i++) {
		fread(b1, 1, 1, fp);
		fread(b4, 1, 4, fp);
	}

	fseek(fp, 454529, SEEK_SET);
	fread(b4, 1, 4, fp);
	fread(b4, 1, 4, fp);
	fseek(fp, -8, SEEK_CUR);

	bigBuf = (unsigned char *)malloc(2609);
	if (!bigBuf) {
		sidecart_puts("malloc(2609) failed\n");
		fclose(fp);
		return 1;
	}

	sidecart_printf("free ST-RAM before big fread: %ld bytes\n", (long)Mxalloc(-1L, 0));
	sidecart_puts("about to fread(2609)\n");
	got = fread(bigBuf, 1, 2609, fp);
	sidecart_printf("fread(2609) returned %u\n", (unsigned)got);

	free(bigBuf);
	fclose(fp);

	sidecart_puts("test.tos: done\n");
	return 0;
}
