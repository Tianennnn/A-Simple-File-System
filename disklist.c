#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <string.h>
#include "emalloc.h"
#include "sfs.h"

boot_t boot_sector;
char *fat_table;
int fat_size = 0;
entry_t root_dir;


/**
 * Function:  process_date
 * --------------------
 * @brief: extract the date from the binary data.
 *
 * @param raw_info: The raw data containing the information.
 * @param  formatted_date: the date in readable format: yyyy/mm/dd.
 *
 */
void process_date(uint16_t raw_info, char* formatted_date){
    int year = ((raw_info & 0b1111111000000000) >> 9) + 1980;
    int month = (raw_info & 0b0000000111100000) >> 5;
    int day = (raw_info & 0b0000000000011111);

    char year_str[5];
    char month_str[3];
    char day_str[3];

    sprintf(year_str, "%d", year);
    sprintf(month_str, "%02d", month);
    sprintf(day_str, "%02d", day);
   
    strcat(formatted_date, year_str);
    strcat(formatted_date, "/");
    strcat(formatted_date, month_str);
    strcat(formatted_date, "/");
    strcat(formatted_date, day_str);
}


/**
 * Function:  process_time
 * --------------------
 * @brief: extract the time from the binary data.
 *
 * @param raw_info: The raw data containing the time.
 * @param  formatted_time: the time in readable format: hh:mm.
 *
 */
void process_time(uint16_t raw_info, char* formatted_time){
    int hour = ((raw_info & 0b1111100000000000) >> 11);
    int minute = (raw_info & 0b0000011111100000) >> 5;
    
    char hour_str[5];
    char minute_str[3];

    sprintf(hour_str, "%02d", hour);
    sprintf(minute_str, "%02d", minute);
    
    strcat(formatted_time, hour_str);
    strcat(formatted_time, ":");
    strcat(formatted_time, minute_str);
}


/**
 * Function:  trimFileName
 * --------------------
 * @brief Process the file name.
 *
 * @param  filename The unformatted name of the file
 * @param  ext The extension of the file
 *
 * @return The formatted file name with extension (if any)
 *
 */
char *trimFileName(char *filename, char *ext) {
    char *new_str;
    new_str = emalloc(13);
    int i, j = 0;
    for (i = 0; i < 8; i++)
        if (filename[i] != 0x20)
            new_str[j++] = filename[i];

    // if the file has an extension
    if (ext[0] != 0x20) {
        new_str[j++] = '.';
        for (i = 0; i < 3; i++)
            if (ext[i] != 0x020)
                new_str[j++] = ext[i];
        new_str[j++] = '\0';
    }
    return new_str;
}


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
uint16_t get_fat(uint16_t i) {
    uint16_t j;

    if (fat_table == NULL || fat_size == 0) {
        /* File system data hasn't loaded yet */
        return 0;
    }
    if (i & 0x01) {
        j = (1 + i * 3) / 2;
        return ((fat_table[j - 1] & 0xF0) >> 4) + (fat_table[j] << 4);
    } else {
        j = i * 3 / 2;
        return ((fat_table[j + 1] & 0x0F) << 8) + (fat_table[j] & 0x0FF);
    }
}


/**
 * Function:  list_dir_entries
 * --------------------
 * @brief list all the files in a directory including sub-directories and the files 
 *        in thesub-directories. 
 *
 * @param fp The pointer to the disk image
 * @param i The address of the sector to search through
 * @param tablesize The number of tab's to put in front of the file listings.
 *
 */
void list_dir_entries(FILE *fp, uint16_t dir_cluster, int tabsize) {
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
                // printf("ssssss:   %d\n", address+j);
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

            for (i = 0; i < tabsize; i++){
                printf("   "); // add spaces to differentiate it from parent parent folder
            }

            // get file creation date
            char date[20] = "";
            process_date(entry.create_date, date);

            // get file creation time
            char time[20] = "";
            process_time(entry.create_time, time);

            char* file_name = trimFileName(entry.filename, entry.extension);
            
            if (entry.attributes & 0x10) { // Subdirectory
                printf("D %10d %-20s %s %s\n", entry.size, entry.filename, date, time);
                for (i = 0; i < tabsize+1; i++) {
                    printf("   "); // add spaces to differentiate it from parent parent folder
                }
                printf("%s\n", entry.filename);
                for (i = 0; i < tabsize + 1; i++) {
                    printf("   "); // add spaces to differentiate it from parent parent folder
                }
                printf("==================\n");
                list_dir_entries(fp, entry.cluster, tabsize + 1);
            }
            else{
                printf("F %10d %-20s %s %s\n", entry.size, file_name, date, time);
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
        fprintf(stderr, "usage: disklist <disk.img>\n");
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

    /* Allocate space for memory copy of the FAT */
    int fat_mem_size;
    fat_size = boot_sector.total_sectors - 33 + 2;
    fat_mem_size = (fat_size & 0x01) ? (fat_size * 3 + 1) / 2 : fat_size * 3 / 2;
    fat_table = emalloc(fat_mem_size);

    /* read a FAT copy */
    fseek(fp, 0x200, SEEK_SET);
    fread(fat_table, fat_mem_size, 1, fp);

    /* Read root directory */
    int start_byte_of_root_dir = 19 * boot_sector.bytes_per_sector;
    fseek(fp, start_byte_of_root_dir, SEEK_SET);
    fread(&root_dir, sizeof(root_dir), 1, fp);

    printf("ROOT\n");
    printf("==================\n");
    list_dir_entries(fp, 0, 0);
    fclose(fp);
}