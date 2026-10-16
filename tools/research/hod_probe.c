// hod_probe — scripted libmgba runner for RAM research (development only).
//
//   hod_probe <rom> <script> <outdir>
//
// Script commands (one per line, '#' comments):
//   run <frames> [KEY,KEY,...]   hold keys for N frames (A B SELECT START RIGHT LEFT UP DOWN R L)
//   tap <KEY,...> [frames]       press for 2 frames (or N) then release for 10
//   save <file> / load <file>    savestate (inside outdir)
//   shot <name>                  framebuffer -> outdir/name.ppm
//   dump <name>                  EWRAM + IWRAM + VRAM + PAL + OAM -> outdir/name.{ewram,iwram,vram,pal,oam}
//   watch on|off                 print GameWork state changes every frame
//   peek                         print a summary line
#include <mgba/core/core.h>
#include <mgba/core/serialize.h>
#include <mgba/core/log.h>
#include <stdarg.h>
#include <mgba-util/vfs.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static struct mCore* core;
static color_t fb[256 * 160];
static char outdir[512];
static int watch;
static unsigned long frame;
static unsigned last_state = 0xFFFFFFFF;

static void nolog(struct mLogger* l, int c, enum mLogLevel lv, const char* f, va_list a) { (void)l; (void)c; (void)lv; (void)f; (void)a; }

static unsigned r8(unsigned a) { return core->busRead8(core, a); }
static unsigned r16(unsigned a) { return core->busRead16(core, a); }
static unsigned r32(unsigned a) { return core->busRead32(core, a); }

static void peek(void) {
    printf("f=%lu mode=%u state=%u sub=%u timer=%u | HP=%u/%u MP=%u/%u H=%u/%u LV=%u gold=%u | 0x0D:%02x 0x0F:%02x\n",
           frame, r8(0x02000048), r8(0x0200000C), r8(0x0200000D), r8(0x0200000E),
           r16(0x0201854E), r16(0x02018786), r16(0x02018550), r16(0x02018788),
           r16(0x02018794), r16(0x0201878A), r8(0x0201877C), r32(0x0201879C),
           r8(0x0200000D), r8(0x0200000F));
}

static void step(unsigned keys) {
    core->setKeys(core, keys);
    core->runFrame(core);
    ++frame;
    if (watch) {
        unsigned s = r8(0x0200000C) | (r8(0x0200000D) << 8) | (r8(0x02000048) << 16);
        if (s != last_state) { peek(); last_state = s; }
    }
}

static unsigned parse_keys(const char* s) {
    static const char* names[] = {"A","B","SELECT","START","RIGHT","LEFT","UP","DOWN","R","L"};
    unsigned k = 0;
    char buf[256];
    if (!s) return 0;
    snprintf(buf, sizeof buf, "%s", s);
    for (char* t = strtok(buf, ","); t; t = strtok(NULL, ","))
        for (int i = 0; i < 10; ++i) if (!strcmp(t, names[i])) k |= 1u << i;
    return k;
}

static void path(char* out, size_t n, const char* name, const char* ext) {
    snprintf(out, n, "%s/%s%s", outdir, name, ext);
}

static void shot(const char* name) {
    char p[600];
    path(p, sizeof p, name, ".ppm");
    FILE* f = fopen(p, "wb");
    fprintf(f, "P6 240 160 255\n");
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 240; ++x) {
            uint32_t c = fb[y * 256 + x];
            unsigned char px[3] = {c & 0xFF, (c >> 8) & 0xFF, (c >> 16) & 0xFF};
            fwrite(px, 1, 3, f);
        }
    fclose(f);
}

static void dump_region(const char* name, const char* ext, unsigned base, unsigned size) {
    char p[600];
    path(p, sizeof p, name, ext);
    FILE* f = fopen(p, "wb");
    for (unsigned i = 0; i < size; ++i) fputc(r8(base + i), f);
    fclose(f);
}

#include <mgba/debugger/debugger.h>

// ---- watchpoint tracing: aggregates unique (pc, lr) per watched access ----
#define MAX_HITS 512
static struct { unsigned pc, lr, addr, count, type; } hits[MAX_HITS];
static int nhits;
static struct mDebugger dbg;

static void on_enter(struct mDebugger* d, enum mDebuggerEntryReason reason, struct mDebuggerEntryInfo* info) {
    unsigned pc = 0, lr = 0;
    core->readRegister(core, "pc", &pc);
    core->readRegister(core, "lr", &lr);
    if (reason == DEBUGGER_ENTER_WATCHPOINT && info) {
        int i;
        for (i = 0; i < nhits; ++i)
            if (hits[i].pc == pc && hits[i].lr == lr) { hits[i].count++; break; }
        if (i == nhits && nhits < MAX_HITS) {
            hits[nhits].pc = pc; hits[nhits].lr = lr; hits[nhits].addr = info->address;
            hits[nhits].count = 1; hits[nhits].type = info->type.wp.accessType; ++nhits;
        }
    }
    d->state = DEBUGGER_RUNNING;
}

static void wp_add(unsigned addr, unsigned len, int type) {
    if (!dbg.platform) {
        memset(&dbg, 0, sizeof dbg);
        dbg.type = DEBUGGER_CUSTOM;
        dbg.entered = on_enter;
        mDebuggerAttach(&dbg, core);
        dbg.state = DEBUGGER_RUNNING;
    }
    for (unsigned i = 0; i < len; ++i) {
        struct mWatchpoint w = {0};
        w.address = addr + i; w.segment = -1; w.type = type;
        dbg.platform->setWatchpoint(dbg.platform, &w);
    }
}

static void wp_report(void) {
    for (int i = 0; i < nhits; ++i)
        printf("hit pc=%08X lr=%08X first=%08X n=%u type=%u\n", hits[i].pc, hits[i].lr, hits[i].addr, hits[i].count, hits[i].type);
    nhits = 0;
}

int main(int argc, char** argv) {
    if (argc < 4) { fprintf(stderr, "usage: hod_probe rom script outdir\n"); return 2; }
    snprintf(outdir, sizeof outdir, "%s", argv[3]);
    static struct mLogger quiet = {.log = nolog};
    mLogSetDefaultLogger(&quiet);
    core = mCoreFind(argv[1]);
    if (!core || !core->init(core)) { fprintf(stderr, "core init failed\n"); return 1; }
    mCoreInitConfig(core, NULL);
    core->setVideoBuffer(core, fb, 256);
    if (!mCoreLoadFile(core, argv[1])) { fprintf(stderr, "load failed\n"); return 1; }
    core->reset(core);

    FILE* sc = fopen(argv[2], "r");
    char line[512];
    while (fgets(line, sizeof line, sc)) {
        char* c = strtok(line, " \t\r\n");
        if (!c || c[0] == '#') continue;
        char* a = strtok(NULL, " \t\r\n");
        char* b = strtok(NULL, " \t\r\n");
        if (!strcmp(c, "run")) {
            int n = atoi(a); unsigned k = parse_keys(b);
            for (int i = 0; i < n; ++i) step(k);
        } else if (!strcmp(c, "tap")) {
            unsigned k = parse_keys(a); int n = b ? atoi(b) : 2;
            for (int i = 0; i < n; ++i) step(k);
            for (int i = 0; i < 10; ++i) step(0);
        } else if (!strcmp(c, "save") || !strcmp(c, "load")) {
            char p[600]; path(p, sizeof p, a, "");
            int save = c[0] == 's';
            struct VFile* vf = VFileOpen(p, save ? O_CREAT | O_TRUNC | O_RDWR : O_RDONLY);
            if (!vf) { fprintf(stderr, "cannot open %s\n", p); continue; }
            if (save) mCoreSaveStateNamed(core, vf, SAVESTATE_SAVEDATA);
            else mCoreLoadStateNamed(core, vf, SAVESTATE_SAVEDATA);
            vf->close(vf);
        } else if (!strcmp(c, "shot")) {
            shot(a);
        } else if (!strcmp(c, "dump")) {
            dump_region(a, ".ewram", 0x02000000, 0x40000);
            dump_region(a, ".iwram", 0x03000000, 0x8000);
            dump_region(a, ".vram", 0x06000000, 0x18000);
            dump_region(a, ".pal", 0x05000000, 0x400);
            dump_region(a, ".oam", 0x07000000, 0x400);
            dump_region(a, ".io", 0x04000000, 0x60);
        } else if (!strcmp(c, "watch")) {
            watch = a && !strcmp(a, "on");
        } else if (!strcmp(c, "poke")) {
            // poke <addr> <len> <byte>  (fill)
            char* v = strtok(NULL, " \t\r\n");
            unsigned addr = strtoul(a, NULL, 0), n = strtoul(b, NULL, 0), val = strtoul(v, NULL, 0);
            for (unsigned i = 0; i < n; ++i) core->busWrite8(core, addr + i, val);
        } else if (!strcmp(c, "pr")) {
            // pr <addr> <len>: hex print
            unsigned addr = strtoul(a, NULL, 0), n = strtoul(b, NULL, 0);
            printf("%08X:", addr);
            for (unsigned i = 0; i < n; ++i) printf(" %02X", r8(addr + i));
            printf("\n");
        } else if (!strcmp(c, "wp")) {
            // wp <addr> <len> <r|w|rw>
            char* v = strtok(NULL, " \t\r\n");
            int t = !v || !strcmp(v, "rw") ? WATCHPOINT_RW : !strcmp(v, "r") ? WATCHPOINT_READ : WATCHPOINT_WRITE;
            wp_add(strtoul(a, NULL, 0), strtoul(b, NULL, 0), t);
        } else if (!strcmp(c, "wpreport")) {
            wp_report();
        } else if (!strcmp(c, "peek")) {
            peek();
        } else if (!strcmp(c, "echo")) {
            printf("--- %s\n", a ? a : "");
        }
        fflush(stdout);
    }
    core->deinit(core);
    return 0;
}
