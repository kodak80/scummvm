// C++ variant of test.c — same code path but compiled as C++ and linked
// against libstdc++ so the C++ runtime and static-constructor machinery
// are exercised exactly the same way scummvm.prg exercises them.
//
// Build: m68k-atari-mintelf-g++ -m68020-60 -O2 -o test.tos test.cpp
//
// If this version reproduces the "Drive P: not responding" dialog while
// the C-only build does not, the culprit is C++ runtime / global-ctor
// activity that happens before main().

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mint/basepage.h>
#include <mint/osbind.h>

#define MONKEY_PATH "P:/MONKEY1/MONKEY.001"

// Pad the binary on disk so TOS streams ~5.5 MB from the host drive at
// Pexec time, matching scummvm.prg's actual disk traffic during program
// load.  Goes into .rodata (initialised, non-zero first byte so the
// linker doesn't move it to .bss).
__attribute__((used)) const unsigned char pad_rodata[5422820] = { 0xab };

// Force a non-trivial C++ static initializer to run before main() so the
// libstdc++ ctor framework is actually wired up.  Vector with N entries
// allocates from the heap during construction, which is similar to what
// some of scummvm's global ctors do.
#include <vector>
#include <string>
struct StaticCtorWitness {
	std::vector<std::string> v;
	StaticCtorWitness() {
		v.reserve(16);
		for (int i = 0; i < 8; i++)
			v.emplace_back("warmup");
	}
};
static StaticCtorWitness g_witness;

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

int main(int argc, char **argv) {
	const char *path = (argc > 1) ? argv[1] : MONKEY_PATH;
	unsigned char b1[1];
	unsigned char b4[4];

	sidecart_printf("test.tos (c++): starting (path='%s')\n", path);
	sidecart_printf("witness ctor saw %u entries\n", (unsigned)g_witness.v.size());

	// Touch pad_rodata so -O2 can't strip it.
	asm volatile("" :: "m"(pad_rodata));

	if (_base) {
		sidecart_printf("basepage=%p tpa=[%p..%p] text=%p+%lx data=%p+%lx bss=%p+%lx\n",
		                (void *)_base,
		                (void *)_base->p_lowtpa, (void *)_base->p_hitpa,
		                (void *)_base->p_tbase, _base->p_tlen,
		                (void *)_base->p_dbase, _base->p_dlen,
		                (void *)_base->p_bbase, _base->p_blen);
	}
	sidecart_printf("free ST-RAM: %ld bytes\n", (long)Mxalloc(-1L, 0));

	FILE *fp = fopen(path, "rb");
	if (!fp) {
		sidecart_printf("fopen('%s') failed\n", path);
		return 1;
	}

	fseek(fp, 0, SEEK_SET);
	fseek(fp, 16, SEEK_SET);

	fread(b1, 1, 1, fp);
	fread(b1, 1, 1, fp);

	fread(b4, 1, 4, fp);
	for (int i = 0; i < 82; i++) {
		fread(b1, 1, 1, fp);
		fread(b4, 1, 4, fp);
	}

	fseek(fp, 454529, SEEK_SET);
	fread(b4, 1, 4, fp);
	fread(b4, 1, 4, fp);
	fseek(fp, -8, SEEK_CUR);

	unsigned char *bigBuf = new unsigned char[2609];
	sidecart_printf("free ST-RAM before big fread: %ld bytes\n", (long)Mxalloc(-1L, 0));
	sidecart_puts("about to fread(2609)\n");
	size_t got = fread(bigBuf, 1, 2609, fp);
	sidecart_printf("fread(2609) returned %u\n", (unsigned)got);

	delete[] bigBuf;
	fclose(fp);

	sidecart_puts("test.tos: done\n");
	return 0;
}
