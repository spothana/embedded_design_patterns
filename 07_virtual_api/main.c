/* =============================================================
 * PATTERN 07 — VIRTUAL API  (Adapter / V-Table)
 * =============================================================
 * CONCEPT
 * -------
 * The Virtual API pattern exposes a stable interface (a struct of
 * function pointers — the "vtable") that the application uses.
 * Different concrete implementations plug into that interface.
 * The application is completely isolated from implementation changes.
 *
 * This is analogous to C++ virtual functions: the vtable is explicit
 * rather than compiler-generated.
 *
 * WHY IT MATTERS IN EMBEDDED SYSTEMS
 * ------------------------------------
 * A hardware redesign often forces a chip swap: the NOR Flash chip
 * on PCB rev1 is discontinued; PCB rev2 uses a different part with
 * a different SPI command set.  With a Virtual API, only a new
 * "adapter" translation unit is needed — the filesystem or
 * bootloader code that calls read/write/erase is unchanged.
 *
 * EXAMPLE SCENARIO
 * ----------------
 * Two NOR Flash chips have different command protocols:
 *   • W25Q64 (old board): page-program cmd = 0x02, read = 0x03
 *   • AT25SF (new board): page-program cmd = 0x12, read = 0x0B (fast-read)
 *
 * The FlashDriver vtable unifies them.  The bootloader calls
 * flash->read() and flash->write() without caring which chip is fitted.
 *
 * KEY MECHANICS
 * -------------
 *  typedef struct FlashDriver { read_fn; write_fn; erase_fn; ... } FlashDriver;
 *  FlashDriver *w25_create();    // returns vtable for W25Q64
 *  FlashDriver *at25_create();   // returns vtable for AT25SF
 *  bootloader_run(FlashDriver *flash);   // same code, any chip
 * ============================================================= */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

/* ── Virtual API (the stable interface) ────────────────────── */
typedef struct FlashDriver FlashDriver;

struct FlashDriver {
    const char *chip_name;
    uint32_t    capacity_bytes;
    uint32_t    page_size;
    uint32_t    sector_size;

    int  (*read  )(FlashDriver *self, uint32_t addr, uint8_t *buf, size_t len);
    int  (*write )(FlashDriver *self, uint32_t addr, const uint8_t *data, size_t len);
    int  (*erase_sector)(FlashDriver *self, uint32_t sector);
    int  (*chip_erase  )(FlashDriver *self);
    void (*destroy)(FlashDriver *self);
};

/* ── Concrete A: W25Q64 ────────────────────────────────────── */
typedef struct {
    FlashDriver base;
    uint8_t     spi_cs_pin;
} W25Q64;

static int w25_read(FlashDriver *self, uint32_t addr, uint8_t *buf, size_t len)
{
    (void)self;
    /* Real: send cmd 0x03, 3-byte addr, clock out len bytes */
    printf("    [W25Q64] CMD=0x03 READ  addr=0x%06X len=%zu\n", addr, len);
    memset(buf, 0xAB, len);   /* simulate data */
    return 0;
}
static int w25_write(FlashDriver *self, uint32_t addr, const uint8_t *data, size_t len)
{
    (void)data;
    /* Real: WRITE_ENABLE then cmd 0x02, addr, data */
    printf("    [%s] CMD=0x06 WREN | CMD=0x02 PAGE_PROG addr=0x%06X len=%zu\n",
           self->chip_name, addr, len);
    return 0;
}
static int w25_erase_sector(FlashDriver *self, uint32_t sector)
{
    uint32_t addr = sector * self->sector_size;
    printf("    [W25Q64] CMD=0x20 SECTOR_ERASE addr=0x%06X (4 kB)\n", addr);
    return 0;
}
static int w25_chip_erase(FlashDriver *self)
{
    (void)self;
    printf("    [W25Q64] CMD=0x60 CHIP_ERASE (~20 s on real HW)\n");
    return 0;
}
static void w25_destroy(FlashDriver *self) { free(self); }

FlashDriver *w25q64_create(uint8_t cs_pin)
{
    W25Q64 *d = calloc(1, sizeof(W25Q64));
    if (!d) return NULL;
    d->spi_cs_pin             = cs_pin;
    d->base.chip_name         = "W25Q64 (8 MB NOR)";
    d->base.capacity_bytes    = 8 * 1024 * 1024;
    d->base.page_size         = 256;
    d->base.sector_size       = 4096;
    d->base.read              = w25_read;
    d->base.write             = w25_write;
    d->base.erase_sector      = w25_erase_sector;
    d->base.chip_erase        = w25_chip_erase;
    d->base.destroy           = w25_destroy;
    return &d->base;
}

/* ── Concrete B: AT25SF641 (different command set) ──────────── */
typedef struct {
    FlashDriver base;
    uint8_t     spi_instance;
} AT25SF;

static int at25_read(FlashDriver *self, uint32_t addr, uint8_t *buf, size_t len)
{
    (void)self;
    /* Fast-read needs one dummy byte after address */
    printf("    [AT25SF] CMD=0x0B FAST_READ addr=0x%06X dummy=1 len=%zu\n",
           addr, len);
    memset(buf, 0xCD, len);
    return 0;
}
static int at25_write(FlashDriver *self, uint32_t addr, const uint8_t *data, size_t len)
{
    (void)self; (void)data;
    /* AT25SF uses 0x12 for page program with 4-byte address mode */
    printf("    [AT25SF] CMD=0x06 WREN | CMD=0x12 PP_4B addr=0x%08X len=%zu\n",
           addr, len);
    return 0;
}
static int at25_erase_sector(FlashDriver *self, uint32_t sector)
{
    uint32_t addr = sector * self->sector_size;
    printf("    [AT25SF] CMD=0x21 BLOCK_ERASE_4K addr=0x%08X\n", addr);
    return 0;
}
static int at25_chip_erase(FlashDriver *self)
{
    (void)self;
    printf("    [AT25SF] CMD=0xC7 CHIP_ERASE\n");
    return 0;
}
static void at25_destroy(FlashDriver *self) { free(self); }

FlashDriver *at25sf_create(uint8_t spi_instance)
{
    AT25SF *d = calloc(1, sizeof(AT25SF));
    if (!d) return NULL;
    d->spi_instance        = spi_instance;
    d->base.chip_name      = "AT25SF641 (8 MB NOR)";
    d->base.capacity_bytes = 8 * 1024 * 1024;
    d->base.page_size      = 256;
    d->base.sector_size    = 4096;
    d->base.read           = at25_read;
    d->base.write          = at25_write;
    d->base.erase_sector   = at25_erase_sector;
    d->base.chip_erase     = at25_chip_erase;
    d->base.destroy        = at25_destroy;
    return &d->base;
}

/* ── Bootloader — uses ONLY FlashDriver* ──────────────────── */
static void bootloader_run(FlashDriver *flash)
{
    printf("  Bootloader using: %s\n", flash->chip_name);
    printf("  Capacity: %u bytes | Page: %u B | Sector: %u B\n\n",
           flash->capacity_bytes, flash->page_size, flash->sector_size);

    uint8_t buf[16];

    /* Step 1: erase first sector */
    flash->erase_sector(flash, 0);

    /* Step 2: write firmware header */
    uint8_t header[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0x01, 0x00 };
    flash->write(flash, 0x000000, header, sizeof(header));

    /* Step 3: read back and verify */
    flash->read(flash, 0x000000, buf, 6);

    printf("    Bootloader done — same code, different chip!\n");
}

int main(void)
{
    printf("=== VIRTUAL API (ADAPTER) — Flash Memory Driver ===\n");
    printf("bootloader_run() is identical for both chips.\n\n");

    printf("  --- PCB Rev 1 (W25Q64) ---\n");
    FlashDriver *old_flash = w25q64_create(4);
    bootloader_run(old_flash);
    old_flash->destroy(old_flash);

    printf("\n  --- PCB Rev 2 (AT25SF641) ---\n");
    FlashDriver *new_flash = at25sf_create(1);
    bootloader_run(new_flash);
    new_flash->destroy(new_flash);

    return 0;
}
