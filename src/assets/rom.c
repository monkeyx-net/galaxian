#include "assets/rom.h"

#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "assets/zip.h"

/* MAME `galaxian` (0.289): name, CRC32, and where the file goes. */
#define ROM_FILES 8
enum { TO_PROGRAM, TO_GFX0, TO_GFX1, TO_PROM };
static const struct {
    const char *mame;
    uint32_t crc;
    unsigned size;
    int to;          /* TO_* */
    unsigned offset; /* within the program ROM ($0000 = 0) */
} files[ROM_FILES] = {
    { "galmidw.u", 0x745e2d61, 0x800, TO_PROGRAM, 0x0000 },
    { "galmidw.v", 0x9c999a40, 0x800, TO_PROGRAM, 0x0800 },
    { "galmidw.w", 0xb5894925, 0x800, TO_PROGRAM, 0x1000 },
    { "galmidw.y", 0x6b3ca10b, 0x800, TO_PROGRAM, 0x1800 },
    { "7l", 0x1b933207, 0x800, TO_PROGRAM, 0x2000 },
    { "1h.bin", 0x39fb43a4, 0x800, TO_GFX0, 0 },
    { "1k.bin", 0x7e3f56a2, 0x800, TO_GFX1, 0 },
    { "6l.bpr", 0xc3ac9467, 0x20, TO_PROM, 0 },
};
#define MAX_FILE_SIZE 0x800

static uint32_t crc32(const uint8_t *p, size_t n)
{
    uint32_t c = 0xFFFFFFFFu;
    while (n--) {
        c ^= *p++;
        for (int k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
    }
    return c ^ 0xFFFFFFFFu;
}

static uint8_t *slot(RomSet *rom, int i)
{
    switch (files[i].to) {
    case TO_GFX0: return rom->gfx[0];
    case TO_GFX1: return rom->gfx[1];
    case TO_PROM: return rom->prom;
    default: return rom->program + files[i].offset;
    }
}

/* Every 2 KB or 32-byte file in a folder, kept where its CRC matches a set file. */
static void scan_folder(RomSet *rom, const char *dir, int *found)
{
    DIR *d = opendir(dir);
    if (!d) return;
    struct dirent *e;
    uint8_t buf[MAX_FILE_SIZE];
    while ((e = readdir(d))) {
        char path[1100];
        snprintf(path, sizeof path, "%s/%s", dir, e->d_name);
        struct stat st;
        if (stat(path, &st) || !S_ISREG(st.st_mode) || (st.st_size != 0x800 && st.st_size != 0x20)) continue;
        FILE *f = fopen(path, "rb");
        if (!f) continue;
        size_t got = fread(buf, 1, (size_t)st.st_size, f);
        fclose(f);
        if (got != (size_t)st.st_size) continue;
        uint32_t crc = crc32(buf, got);
        for (int i = 0; i < ROM_FILES; i++)
            if (!found[i] && crc == files[i].crc && got == files[i].size) {
                memcpy(slot(rom, i), buf, got);
                found[i] = 1;
            }
    }
    closedir(d);
}

int rom_load(RomSet *rom, const char *src, char *err, int errlen)
{
    memset(rom, 0, sizeof *rom);
    struct stat st;
    if (stat(src, &st) != 0) {
        snprintf(err, (size_t)errlen, "%s: not found", src);
        return -1;
    }
    int found[ROM_FILES] = { 0 };
    if (S_ISREG(st.st_mode)) {
        for (int i = 0; i < ROM_FILES; i++) {
            uint8_t *data;
            size_t len;
            int rc = zip_read_crc(src, files[i].crc, &data, &len, err, errlen);
            if (rc == -1) return -1;
            if (rc == 0) {
                if (len == files[i].size) {
                    memcpy(slot(rom, i), data, len);
                    found[i] = 1;
                }
                free(data);
            }
        }
    } else {
        scan_folder(rom, src, found);
    }
    for (int i = 0; i < ROM_FILES; i++)
        if (!found[i]) {
            snprintf(err, (size_t)errlen, "%s: missing %s (CRC %08x); the MAME 'galaxian' set is required", src,
                     files[i].mame, (unsigned)files[i].crc);
            memset(rom, 0, sizeof *rom);
            return -1;
        }
    rom->loaded = 1;
    return 0;
}

int rom_byte(const RomSet *rom, unsigned addr)
{
    if (!rom->loaded || addr >= ROM_PROGRAM_SIZE) return -1;
    return rom->program[addr];
}

/* want_dir: a folder; else a regular file */
static int exists(const char *path, int want_dir)
{
    struct stat st;
    return stat(path, &st) == 0 && (want_dir ? S_ISDIR(st.st_mode) : S_ISREG(st.st_mode));
}

int rom_locate(const char *argv0, char *out, size_t outlen)
{
    const char *env = getenv("GALAXIAN_ROM");
    if (env && *env && (exists(env, 0) || exists(env, 1))) {
        snprintf(out, outlen, "%s", env);
        return 0;
    }
    char exe_dir[900] = ".";
    if (argv0) {
        snprintf(exe_dir, sizeof exe_dir, "%s", argv0);
        char *slash = strrchr(exe_dir, '/');
#ifdef _WIN32
        char *bslash = strrchr(exe_dir, '\\');
        if (bslash > slash) slash = bslash;
#endif
        if (slash) *slash = '\0';
        else snprintf(exe_dir, sizeof exe_dir, ".");
    }
    const char *const dirs[2] = { exe_dir, "." };
    /* the folder must be a folder: the program itself is called galaxian too */
    const char *const names[2] = { "galaxian.zip", "galaxian" };
    for (int d = 0; d < 2; d++)
        for (int n = 0; n < 2; n++) {
            snprintf(out, outlen, "%s/%s", dirs[d], names[n]);
            if (exists(out, n == 1)) return 0;
        }
#ifdef GALAXIAN_SOURCE_DIR
    for (int n = 0; n < 2; n++) { /* development builds: the source tree's roms/ */
        snprintf(out, outlen, "%s/roms/%s", GALAXIAN_SOURCE_DIR, names[n]);
        if (exists(out, n == 1)) return 0;
    }
#endif
    out[0] = '\0';
    return -1;
}
