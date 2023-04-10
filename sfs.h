#include <stdint.h>

/*
 * The boot sector.
 */
typedef struct {
  char      _a[3];               /* 3 reserved bytes used for a JMP instruction. */
  char      name[8];             /* The OEM name of the volume. */
  uint16_t  bytes_per_sector;    /* The number of bytes per sector. */
  uint8_t   sectors_per_cluster; /* The number of sectors per cluster. */
  uint16_t  reserved_sectors;    /* The number of reserved sectors. */
  uint8_t   fats;                /* The number of file allocation tables. */
  uint16_t  root_entries;        /* The number of entries in the root directory. */
  uint16_t  total_sectors;       /* The number of hard disk sectors. If 0, use total_sectors2. */
  uint8_t   media_descriptor;    /* The media descriptor. */
  uint16_t  sectors_per_fat;     /* The number of sectors per FAT */
  uint16_t  sectors_per_track;   /* The number of sectors per track. */
  uint16_t  heads;               /* The number of hard disk heads. */
  uint32_t  hidden_sectors;      /* The number of hidden sectors. */
  uint32_t  total_sectors2;      /* The number of hard disk sectors. */
  uint8_t   drive_index;         /* The drive index. */
  uint8_t   _b;                  /* Reserved. */
  uint8_t   signature;           /* The extended boot signature. */
  uint32_t  id;                  /* The volume ID. */
  char      label[11];           /* The partition volume label. */
  char      type[8];             /* The file system type. */
  uint8_t   _c[448];             /* Code to be executed. */
  uint16_t  sig;                 /* The boot signature. Always 0xAA55. */
} __attribute__ ((packed)) boot_t;

/*
 * Directory Entry.
 */
typedef struct {
  char      filename[8];         /* The file name. */
  char      extension[3];        /* The file extension. */
  uint8_t   attributes;          /* File attributes. */
  uint8_t   _a;                  /* Reserved. */
  uint8_t   create_time_us;      /* The microsecond value of the creation time. */
  uint16_t  create_time;         /* The creation time. */
  uint16_t  create_date;         /* The creation date. */
  uint16_t  last_access_date;    /* The date the file was last accessed. */
  uint8_t   _b[2];               /* Reserved. */
  uint16_t  last_modified_time;  /* The time the file was last modified. */
  uint16_t  last_modified_date;  /* The date the file was last modified. */
  uint16_t  cluster;             /* The cluster containing the start of the file. */
  uint32_t  size;                /* The file size in bytes. */
} __attribute__ ((packed)) entry_t;

/*
 * Struct to read 2 FAT entries.
 */
typedef struct {
   uint8_t  b0;
   uint8_t  b1;
   uint8_t  b2;
} __attribute__ ((packed)) fat_entry_t;

