#include "emalloc.h"
#include "sfs.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>

boot_t boot_sector;
char *fat_table;
int fat_size = 0;
entry_t root_dir;

/**
 * Function:  trimFileName
 * --------------------
 * @brief Process the file name
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
 * Function:  get_file_entry_in_root
 * --------------------
 * @brief get the file entry stored in the root directory.
 *
 * @param  fp: a pointer to the disk 
 * @param  start_byte: the start byte of the root directory
 * @param  file_name: the name of the file to retrive from the root directory 
 *
 * @return The file entry contains the info of the file.
 * 
 */
entry_t get_file_entry_in_root(FILE *fp, int start_byte, char* file_name){
    entry_t cur_entry;
    int end_byte_of_root_dir = start_byte + 14 * boot_sector.bytes_per_sector;

    while (start_byte<end_byte_of_root_dir){
        fseek(fp, start_byte, SEEK_SET);
        fread(&cur_entry, sizeof(cur_entry), 1, fp);

        if ((uint8_t)cur_entry.filename[0] == 0x00) { // free entry & no more
            printf(" File not found.\n");
            exit(-1);
        }
        if ((uint8_t)cur_entry.filename[0] == 0xE5) {
            start_byte += 32;
            continue; // this entry is free
        }
        if (cur_entry.attributes == 0x0F) {
            start_byte += 32;
            continue; // skip long file name
        }
        if (cur_entry.attributes & 0x10) {
            start_byte += 32;
            continue; // skip directories
        }

        char* cur_file_name = trimFileName(cur_entry.filename, cur_entry.extension);
        if (strcmp(cur_file_name, file_name)==0) {
            return cur_entry;
        }
        start_byte += 32;
    }
}


/**
 * Function:  get_file
 * --------------------
 * @brief copy 512 bytes of data of the file to local current directory
 *
 * @param  cur_sector: the index of the physical sector to copy data from. 
 * @param  remain_sectors: the number of remaining sectors to copy data from.
 * @param  fp: a pointer to the disk
 * @param  new: a pointer to the file to copy data to
 * @param  total_size: total size of the file to be copied.
 * 
 */
void get_file(int cur_sector, int remain_sectors, FILE *fp, FILE *new, uint32_t total_size) {
    int address = cur_sector * 512;
    remain_sectors -= 1;
    char content[512];
    fseek(fp, address, SEEK_SET);
    if (remain_sectors==0) {    // if reach the last sector where the file is stored
        fread(&content, total_size % 512, 1, fp); // only read the remaining file
        fwrite(&content, total_size%512, 1, new);
    }else{
        fread(&content, 512, 1, fp);
        fwrite(&content, 512, 1, new);
        int next_sector = get_fat(cur_sector - 33 + 2) + 33 - 2;
        get_file(next_sector, remain_sectors, fp, new, total_size);
    }
}


int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "usage: diskget <disk.img> <filename>\n");
        exit(-1);
    }

    // convert the filename to uppercase
    char* file_name = argv[2];
    int i =0;
    while (file_name[i] != '\0'){
        file_name[i] = toupper(file_name[i]);
        i++;
    }

    FILE *new;
    // check if a file of the same name is already in the local directory.
    if ((new = fopen(argv[2], "r")) != NULL) {
        fclose(new);
        printf("There is a file of the same name in the local directory.\n");
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

    int start_byte_of_root_dir = 19 * boot_sector.bytes_per_sector;
    entry_t root_file_entry = get_file_entry_in_root(fp, start_byte_of_root_dir, file_name);

    new = fopen(file_name, "w+");
    int first_sector = 33 + root_file_entry.cluster -2;
    uint32_t file_size = root_file_entry.size;
    int occupied_sectors = file_size / boot_sector.bytes_per_sector + (file_size % boot_sector.bytes_per_sector != 0);

    get_file(first_sector, occupied_sectors, fp, new, file_size);

    fclose(new);
    fclose(fp);
}