#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include "emalloc.h"
#include "sfs.h"

boot_t boot_sector;
entry_t root_dir;
fat_entry_t fat;

char *fat_table;
int fat_size = 0;
int file_count = 0;

/**
 * Function:  get_fat
 * --------------------
 * @brief get the value of the FAT entry.
 *
 * @param i The index of the FAT entry.
 *
 * @return The value of the FAT entry.
 *
 */
int get_fat(int i) {
    int j;

    if (fat_table == NULL || fat_size == 0) {
        /* File system data hasn't loaded yet */
        return -1;
    }
    if (i & 0x01) {
        j = (1 + i * 3) / 2;
        return ((fat_table[j - 1] & 0xF0) >> 4) + (fat_table[j] << 4);
    } else {
        j = i * 3 / 2;
        return ((fat_table[j + 1] & 0x0F) << 8) + fat_table[j];
    }
}

/**
 * Function:  getFreeBlocks
 * --------------------
 * @brief get the number of unused sectors by looking at the FAT entries.
 *
 * @return: The number of unused sectors.
 *
 */
int getFreeBlocks() {
    int i, free_blocks = 0;

    if (fat_table == NULL || fat_size == 0) {
        /* File system data hasn't loaded yet */
        return -1;
    }

    // the first two entries in FAT are reserved
    for (i = 2; i < fat_size; i++) {
        if (get_fat(i) == 0) {
            // 0x000 in an FAT entry means unused
            free_blocks += 1;
        }
    }
    return free_blocks;
}

/**
 * Function:  count_files_in_dir
 * --------------------
 * @brief: get the number of files in the directory.
 *
 * @param fp: a pointer to the disk containing the directory.
 * @param  dir_cluster: the logical sector of the directory.
 * 
 * @return: The number of files in the directory.
 *
 */
void count_files_in_dir(FILE *fp, uint16_t dir_cluster) {
    entry_t entry;
    char *buf;
    int i, j;
    uint16_t local_dir_cluster = dir_cluster;
    uint32_t address = dir_cluster == 0 ? 0x2600 : (dir_cluster + 33 - 2) * 512;

    buf = emalloc(boot_sector.bytes_per_sector);
    // for each sector
    for (;;) {
        fseek(fp, address, SEEK_SET);
        fread(buf, boot_sector.bytes_per_sector, 1, fp);
        // for each entry in the sector
        for (j = 0; j < boot_sector.bytes_per_sector; j += sizeof(entry_t)) {
            memcpy(&entry, buf + j, sizeof(entry_t));

            if ((uint8_t)entry.filename[0] == 0x00)
                return; // free entry & no more
            if ((uint8_t)entry.filename[0] == 0xE5)
                continue; // this entry is free
            if (entry.attributes == 0x0F)
                continue; // skip long file name
            if ((uint8_t)entry.filename[0] == 0x2E)
                continue; // skip . & .. entries
            if (entry.cluster<2){ // skip entry with the first logical sector to be 0 or 1
                continue;
            }

            if (entry.attributes & 0x10) { // Subdirectory
                count_files_in_dir(fp, entry.cluster);
            } else {
                file_count += 1;
            }
        }
        if (local_dir_cluster == 0) { // root directory
            address += boot_sector.bytes_per_sector;
        } else {
            if (get_fat(local_dir_cluster) >= (uint16_t)0x0FF8 || get_fat(local_dir_cluster) == 0xFF) {
                break; // no more sectors
            } else {
                address = (get_fat(local_dir_cluster) + 33 - 2) * 512;
                local_dir_cluster = get_fat(local_dir_cluster);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "usage: diskinfo <disk.img>\n");
        exit(-1);
    }

    FILE *fp;

    if ((fp = fopen(argv[1], "r")) == NULL) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        exit(1);
    }

    /* Read boot sector */
    fseek(fp, 0x0, SEEK_SET);
    fread(&boot_sector, sizeof(boot_sector), 1, fp);

    char os_name[9];
    for(int i =0;i<8;i++){
        os_name[i] = boot_sector.name[i];
    }
    os_name[8]='\0';
    int disk_size = boot_sector.total_sectors * boot_sector.bytes_per_sector;

    /* Read root directory */
    int start_byte_of_root_dir = 19 * boot_sector.bytes_per_sector;
    int end_byte_of_root_dir = start_byte_of_root_dir + 14 * boot_sector.bytes_per_sector;
    fseek(fp, start_byte_of_root_dir, SEEK_SET);
    fread(&root_dir, sizeof(root_dir), 1, fp);

    // In the root directory, find the directory entry with attribute 0X08
    entry_t dir;
    dir.attributes = 0;
    int start_byte_of_entry  = start_byte_of_root_dir;
    while(dir.attributes!= 0x08 && start_byte_of_entry<end_byte_of_root_dir){
        start_byte_of_entry += 32;       // go to the next entry
        fseek(fp, start_byte_of_entry, SEEK_SET);
        fread(&dir, sizeof(dir), 1, fp);
    }
    char label[9];
    for(int i =0;i<8;i++){
        label[i] = dir.filename[i];
    }
    label[8] = '\0';

    int fat_mem_size;
    /* Allocate space for memory copy of the FAT */
    fat_size = boot_sector.total_sectors - 33 + 2;
    fat_mem_size = (fat_size & 0x01) ? (fat_size*3+1)/2 : fat_size*3/2;
    fat_table = emalloc(fat_mem_size);
    /* read a FAT copy */
    fseek(fp, 0x200, SEEK_SET);
    fread(fat_table, fat_mem_size, 1, fp);

    // get free size of the disk
    int freeBlocks = getFreeBlocks();
    if(freeBlocks == -1){
        printf("File system data hasn't loaded yet");
        exit(-1);
    }
    int free_disk_size = freeBlocks * boot_sector.bytes_per_sector;

    // get the number of files
    count_files_in_dir(fp,0);

    int FAT_num = boot_sector.fats;
    int sectors_per_FAT = boot_sector.sectors_per_fat;

    fclose(fp);
    
    // print the statistics of the disk image 
    printf("OS Name: %s\n", os_name);
    printf("Label of the disk: %s\n", label);
    printf("Total size of the disk: %d\n", disk_size);
    printf("Free size of the disk: %d\n", free_disk_size);
    printf("The number of files in the disk: %d\n", file_count);
    printf("Number of FAT copies: %d\n", FAT_num);
    printf("Sectors per FAT: %d\n", sectors_per_FAT);

}