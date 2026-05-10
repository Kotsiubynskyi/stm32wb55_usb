/*
 * msc_disk.c — FAT12 RAM disk backing store for TinyUSB Mass Storage class.
 *
 * Disk geometry
 * -------------
 *   Total size  : 64 KB  (128 sectors × 512 bytes)
 *   Filesystem  : FAT12
 *   Cluster size: 1 sector (512 bytes) — smallest possible, suits tiny disks
 *
 * Sector map
 * ----------
 *   Sector 0        Boot sector (BPB — BIOS Parameter Block)
 *   Sector 1        FAT1 (File Allocation Table, primary copy)
 *   Sector 2        FAT2 (FAT duplicate required by spec)
 *   Sector 3        Root directory (16 entries × 32 bytes = 512 bytes)
 *   Sectors 4–127   Data area — clusters 2 through 125 (124 × 512 B = 62 KB)
 *
 * FAT12 cluster numbering starts at 2; clusters 0 and 1 are reserved.
 * The first data cluster (cluster 2) therefore lives at sector 4:
 *   first_data_sector = reserved(1) + FATs×FAT_size(2×1) + root_dir(1) = 4
 *
 * Memory cost: 128 × 512 = 65 536 bytes in BSS (zero-initialised SRAM).
 * STM32WB55 has 196 KB of application RAM, so this is ~33 % — fine for a demo.
 *
 * Persistence: none.  Contents are lost on every power cycle / reset.
 */

#include "tusb.h"
#include <string.h>

#define DISK_SECTOR_COUNT 128   /* total number of 512-byte sectors   */
#define DISK_SECTOR_SIZE  512   /* bytes per sector (standard for FAT) */

/* The entire disk lives in BSS; zero-initialised at startup. */
static uint8_t disk[DISK_SECTOR_COUNT][DISK_SECTOR_SIZE];

/* ------------------------------------------------------------------ */
/* FAT12 initialisation                                                */
/* ------------------------------------------------------------------ */

/*
 * disk_init — write the minimal FAT12 metadata into the disk array.
 *
 * Only sectors 0–3 need explicit values; the data area (sectors 4+) is
 * already zero, which FAT12 interprets as "all clusters free".
 */
static void disk_init(void) {

  /* ---- Sector 0: Boot sector / BPB -------------------------------- */
  uint8_t *boot = disk[0];

  /* Bytes 0-2: x86 short-jump + NOP.  Required by Windows/Linux FAT
   * parsers even though we never execute boot code.                   */
  boot[0] = 0xEB; boot[1] = 0x3C; boot[2] = 0x90;

  /* Bytes 3-10: OEM name — 8 ASCII bytes, space-padded.              */
  memcpy(&boot[3], "MSDOS5.0", 8);

  /* Bytes 11-12: Bytes Per Sector — must be 512, 1024, 2048 or 4096.
   * Stored little-endian; 0x0200 = 512.                              */
  boot[11] = 0x00; boot[12] = 0x02;

  /* Byte 13: Sectors Per Cluster.  1 means each cluster is one sector.
   * Larger values reduce FAT overhead but waste space on small files. */
  boot[13] = 0x01;

  /* Bytes 14-15: Reserved Sector Count — number of sectors before
   * FAT1, including the boot sector itself.  Always 1 for FAT12.     */
  boot[14] = 0x01; boot[15] = 0x00;

  /* Byte 16: Number of FATs.  The spec mandates 2 for redundancy.    */
  boot[16] = 0x02;

  /* Bytes 17-18: Root Entry Count — maximum directory entries in the
   * root directory.  16 entries × 32 bytes = 512 bytes = 1 sector.   */
  boot[17] = 0x10; boot[18] = 0x00;

  /* Bytes 19-20: Total Sectors (16-bit).  Used when disk < 32 MB.
   * 128 sectors = 0x0080, stored little-endian.                      */
  boot[19] = DISK_SECTOR_COUNT; boot[20] = 0x00;

  /* Byte 21: Media Type.
   *   0xF8 = fixed (non-removable) disk.
   *   0xF0 = removable (e.g. floppy).
   * Must match FAT[0] & 0xFF (see FAT initialisation below).         */
  boot[21] = 0xF8;

  /* Bytes 22-23: Sectors Per FAT.  1 sector holds 512 bytes; each
   * FAT12 entry is 1.5 bytes, so 1 sector covers 512/1.5 ≈ 341
   * entries — more than enough for 128 clusters.                     */
  boot[22] = 0x01; boot[23] = 0x00;

  /* Bytes 24-25: Sectors Per Track.  Geometry hint for legacy BIOS.
   * Irrelevant for USB mass storage; set to 1.                       */
  boot[24] = 0x01; boot[25] = 0x00;

  /* Bytes 26-27: Number of Heads.  Same — irrelevant, set to 1.      */
  boot[26] = 0x01; boot[27] = 0x00;

  /* Bytes 28-31: Hidden Sectors before this partition — 0.           */
  /* Bytes 32-35: Total Sectors (32-bit) — 0 when 16-bit field used.  */
  /* (already zero from BSS)                                          */

  /* Byte 36: BIOS Drive Number (0x80 = first hard disk).             */
  boot[36] = 0x80;

  /* Byte 37: Reserved1 — must be 0.                                  */
  boot[37] = 0x00;

  /* Byte 38: Extended Boot Signature — 0x29 indicates that the next
   * three fields (Volume ID, label, FS type) are present.            */
  boot[38] = 0x29;

  /* Bytes 39-42: Volume Serial Number — arbitrary 32-bit value.
   * Windows uses this to track which disk is mounted.                */
  boot[39] = 0xDE; boot[40] = 0xAD; boot[41] = 0xBE; boot[42] = 0xEF;

  /* Bytes 43-53: Volume Label — 11 bytes, space-padded.  This string
   * appears as the drive name in file managers.                      */
  memcpy(&boot[43], "RAMDISK    ", 11);

  /* Bytes 54-61: File System Type — 8 bytes, space-padded.  Purely
   * informational; the OS determines FAT type from the BPB math.     */
  memcpy(&boot[54], "FAT12   ", 8);

  /* Bytes 510-511: Boot sector signature — required by all FAT specs. */
  boot[510] = 0x55; boot[511] = 0xAA;


  /* ---- Sector 1: FAT1 --------------------------------------------- */
  /*
   * FAT12 packs two 12-bit entries into 3 bytes (little-endian nibble
   * order).  The first two entries are reserved:
   *
   *   Entry 0: media descriptor = 0xFF8  (0xF8 | 0xF00)
   *   Entry 1: end-of-chain mark = 0xFFF
   *
   * Packed into 3 bytes:
   *   byte 0 = low  8 bits of entry 0         → 0xF8
   *   byte 1 = high 4 bits of entry 0 (low nibble)
   *          | low  4 bits of entry 1 (high nibble) → 0xFF
   *   byte 2 = high 8 bits of entry 1         → 0x0F
   *
   * All remaining bytes are 0x00 = free clusters.
   */
  uint8_t *fat = disk[1];
  fat[0] = 0xF8; fat[1] = 0xFF; fat[2] = 0xFF;


  /* ---- Sector 2: FAT2 (mandatory copy of FAT1) -------------------- */
  memcpy(disk[2], disk[1], DISK_SECTOR_SIZE);


  /* ---- Sector 3: Root directory ------------------------------------ */
  /*
   * Each directory entry is 32 bytes.  We add a single volume-label
   * entry so the disk name shown in the file manager matches the BPB
   * label.  All other entries remain zero (empty).
   *
   * Entry layout (bytes 0-31):
   *   0-10   Name+extension (space-padded, no dot separator)
   *   11     Attributes: 0x08 = ATTR_VOLUME_ID
   *   12-31  Timestamps, cluster, size — all 0 for a label entry
   */
  uint8_t *root = disk[3];
  memcpy(&root[0], "RAMDISK    ", 11);  /* matches boot[43] */
  root[11] = 0x08;                       /* ATTR_VOLUME_ID   */
}

/* ------------------------------------------------------------------ */
/* Internal helper                                                     */
/* ------------------------------------------------------------------ */

static bool ready = false;

/* Initialise once on first SCSI access so the USB stack is fully up
 * before we touch the disk array.                                    */
static bool ensure_ready(void) {
  if (!ready) {
    disk_init();
    ready = true;
  }
  return true;
}

/* ------------------------------------------------------------------ */
/* TinyUSB MSC device callbacks                                        */
/* ------------------------------------------------------------------ */

/*
 * Called during SCSI INQUIRY.  Returns the vendor/product strings that
 * appear in `lsusb -v` and in dmesg ("usb-storage: … Product: …").
 */
void tud_msc_inquiry_cb(uint8_t lun, uint8_t vendor_id[8],
                        uint8_t product_id[16], uint8_t product_rev[4]) {
  (void)lun;
  memcpy(vendor_id,   "STM32   ", 8);
  memcpy(product_id,  "RAM Disk        ", 16);
  memcpy(product_rev, "1.0 ", 4);
}

/*
 * Called for SCSI TEST UNIT READY.  Return true when the medium is
 * present and the device is ready to accept I/O.  Returning false
 * causes the host to retry and eventually report "no medium".
 */
bool tud_msc_test_unit_ready_cb(uint8_t lun) {
  (void)lun;
  return ensure_ready();
}

/*
 * Called for SCSI READ CAPACITY.  The host uses these values to
 * compute the usable disk size shown in the file manager.
 */
void tud_msc_capacity_cb(uint8_t lun, uint32_t *block_count,
                         uint16_t *block_size) {
  (void)lun;
  ensure_ready();
  *block_count = DISK_SECTOR_COUNT;
  *block_size  = DISK_SECTOR_SIZE;
}

/*
 * Called for SCSI READ(10).  TinyUSB may split a single host request
 * into several callback invocations with different offsets; bufsize
 * tells us exactly how many bytes to copy this call.
 */
int32_t tud_msc_read10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                          void *buffer, uint32_t bufsize) {
  (void)lun;
  if (lba >= DISK_SECTOR_COUNT) return -1;
  memcpy(buffer, disk[lba] + offset, bufsize);
  return (int32_t)bufsize;
}

/*
 * Called for SCSI WRITE(10).  Writes go straight into the RAM array;
 * they are visible immediately on subsequent reads but are lost on
 * reset.
 */
int32_t tud_msc_write10_cb(uint8_t lun, uint32_t lba, uint32_t offset,
                           uint8_t *buffer, uint32_t bufsize) {
  (void)lun;
  if (lba >= DISK_SECTOR_COUNT) return -1;
  memcpy(disk[lba] + offset, buffer, bufsize);
  return (int32_t)bufsize;
}

/*
 * Called for any SCSI command not handled above (e.g. MODE SENSE,
 * PREVENT/ALLOW MEDIUM REMOVAL).  Returning -1 makes TinyUSB respond
 * with CHECK CONDITION / ILLEGAL REQUEST, which is acceptable for
 * commands we don't implement.
 */
int32_t tud_msc_scsi_cb(uint8_t lun, uint8_t const scsi_cmd[16],
                        void *buffer, uint16_t bufsize) {
  (void)lun; (void)scsi_cmd; (void)buffer; (void)bufsize;
  return -1;
}
