#include "emalloc.h"
#include "sfs.h"
#include <ctype.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>

boot_t boot_sector;
char *fat_table;
int fat_size = 0;
entry_t root_dir;

FILE *disk;
FILE *file;

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
    if (i & 0x01) {     // odd
        j = (1 + i * 3) / 2;
        return ((fat_table[j - 1] & 0xF0) >> 4) + (fat_table[j] << 4);
    } else {            // even
        j = i * 3 / 2;  
        return ((fat_table[j + 1] & 0x0F) << 8) + (fat_table[j] & 0x0FF);
    }
}


/**
 * Function:  update_fat
 * --------------------
 * @brief update the FAT entry.
 *
 * @param i The index of the FAT entry to be updated.
 * @param new_val the new value of the FAT entry
 *
 */
void update_fat(uint16_t i, uint16_t new_val) {
    new_val = new_val & 0xFFF;
    uint16_t j;

    if (i & 0x01) {     //odd
        j = (1 + i * 3) / 2;
        fat_table[j]= (new_val>> 4) & 0xFF;
        fat_table[j-1] =  ((new_val & 0x0F) << 4) + (fat_table[j-1]);
    } else {            //even
        j = i * 3 / 2;
        fat_table[j]= new_val & 0xFF;
        fat_table[j+1]= ((new_val >> 8 ) & 0x0F) + (fat_table[j+1]);
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
 * Function:  get_free_entry_in_root_dir
 * --------------------
 * @brief Scan through every used entry in the root directory and check if there is a file of 
 *        the same name in the disk. If not, return a free entry in the root directory.
 *        
 * @param disk: a pointer the disk 
 * @param file_name: the name of the file to be put in the disk.  
 * @param free_entry_address: the address of the free_entry
 *
 */
void get_free_entry_in_root_dir (FILE* disk, char* file_name, int* free_entry_address){
    int start_byte_of_root_dir = 19 * boot_sector.bytes_per_sector;
    int start_byte = start_byte_of_root_dir;
    int end_byte_of_root_dir = start_byte_of_root_dir + 14* boot_sector.bytes_per_sector;
    while (start_byte<end_byte_of_root_dir){    // for every entry in the root directory
        entry_t cur_entry;
        fseek(disk, start_byte, SEEK_SET);
        fread(&cur_entry, sizeof(cur_entry), 1, disk);

        if ((uint8_t)cur_entry.filename[0] == 0x00) { // free entry & no more
            *free_entry_address = start_byte;
            return; 
        }
        if ((uint8_t)cur_entry.filename[0] == 0xE5){    // this entry is free
            *free_entry_address = start_byte;
            start_byte += 32;
            continue; 
        }
        char* cur_file_name = trimFileName(cur_entry.filename, cur_entry.extension);
        if (strcmp(cur_file_name, file_name)==0) {
            printf("There is a file of the same name in the disk.\n");
            fclose(disk);
            fclose(file);
            exit(-1);
        }
        start_byte += 32;
    }
}


/**
 * Function:  get_free_sub_dir_entries
 * --------------------
 * @brief Scan through every used entry in all directory and check if there is a file of 
 *        the same name in the disk. If not, return a free entry in the destination directory.
 *        
 * @param fp: a pointer to the disk
 * @param dir_cluster: the logical sector in the directory.
 * @param destination: the name of the directory to scan for free entry.
 * @param free_entry_address: the address of the free entry.
 * @param file_name: the name of the file to be put into the disk. 
 * @param cur_dir_name: the name of the current scanning directory.
 *
 */
void get_free_sub_dir_entries(FILE *fp, uint16_t dir_cluster, char* destination, int* free_entry_address, char* file_name, char* cur_dir_name) {
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

            if ((uint8_t)entry.filename[0] == 0x00){    // free entry & no more
                if(strcmp(destination,cur_dir_name) == 0){
                    *free_entry_address = address + j;
                }
                return;
            }
            if ((uint8_t)entry.filename[0] == 0xE5){    // this entry is free
                if(strcmp(destination,cur_dir_name) == 0){
                    *free_entry_address = address + j;
                }
                continue; 
            }
            if (entry.attributes == 0x0F){
                continue; // skip long file name
            }
            if ((uint8_t)entry.filename[0] == 0x2E){
                continue; // skip . & .. entries
            }
            
            if (entry.attributes & 0x10) { // Subdirectory
                get_free_sub_dir_entries(fp, entry.cluster, destination, free_entry_address, file_name, entry.filename);
            }
            else{
                char* cur_file_name = trimFileName(entry.filename, entry.extension);
                if (strcmp(cur_file_name, file_name)==0){
                    printf("There is a file of the same name in the disk.\n");
                    fclose(disk);
                    fclose(file);
                    exit(-1);
                }
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


/**
 * Function:  fill_info_to_entry
 * --------------------
 * @brief fill the size of the file, the file name, the file extension and 
 *        the first_physical_sector to the file entry.
 *        
 * @param file: a pointer to the file to be put into the disk
 * @param file_name: the name of the file to be put into the disk. 
 * @param first_physical_sector: the first physical sector of the file data to be put in the disk.
 * 
 * @return The partly filled file entry
 */
entry_t fill_info_to_entry (FILE* file, char* file_name, int first_physical_sector) {
    // initialize an entry with the attributes are all 0;
    entry_t entry = {0};
    // get the size of the file
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    entry.size = file_size;
    
    int i = 0;
    // initialize entry.filename and entry.extension with spaces
    while(i<3){
        entry.extension[i] = ' ';
        entry.filename[i] = ' ';
        i++;
    }
    while(i<8){
        entry.filename[i] = ' ';
        i++;
    }

    // store the name and extension to the entry
    i = 0;
    int any_extension = 1;  // 0 for False, 1 for True
    while (file_name[i]!='.'){
        if(i==8){
            any_extension = 0;
            break;
        }
        entry.filename[i] = file_name[i];
        i++;
    }
    if(any_extension==1){
        i++;
        int j=0;
        while(file_name[i]!='\0'){
            entry.extension[j] = file_name[i];
            i++;
            j++;
        }
    }

    // store the first physical sector
    entry.cluster = first_physical_sector - 33 +2;
    return entry;
}


/**
 * Function:  get_free_sector
 * --------------------
 * @brief  get a free sector in the data area of the disk.
 *        
 *
 * @param file_name: the name of the file to be put into the disk. 
 * @param first_physical_sector: the first physical sector of the file data to be put in the disk.
 * 
 * @return the free sector in the data area of the disk.
 * 
 */
int get_free_sector(){
    int data_area_start_sector = 33;
    int start_sector = data_area_start_sector;
    int start_address = 33 * boot_sector.bytes_per_sector;
    while(get_fat(start_sector+33-2)!=0x00){
        start_sector+=1;
        start_address += boot_sector.bytes_per_sector;
    }
    return start_sector;
}


/**
 * Function:  put_in_data_area
 * --------------------
 * @brief store 512 bytes of data of the file to the sector.
 *        
 * @param cur_physical_sector: the sector to store data in.
 * @param sectors_needed: the number of sectors still needed to store all data of the file.
 * @param total_size: total size of the file to be stored.
 * 
 */
void put_in_data_area (int cur_physical_sector, int sectors_needed, int total_size){
    int address = cur_physical_sector * 512;
    sectors_needed -= 1;
    char content[512];
    fseek(disk, address, SEEK_SET);

    if (sectors_needed==0) {    // if putting data to the last sector
        fread(&content, total_size % 512, 1, file); // only read the remaining file
        fwrite(&content, total_size%512, 1, disk);
        update_fat(cur_physical_sector - 33 + 2, 0XFFF);
    }else{
        fread(&content, 512, 1, file);
        fwrite(&content, 512, 1, disk);
        int next_sector = get_free_sector();
        int next_address = (next_sector -33 + 2) * boot_sector.bytes_per_sector;
        update_fat(cur_physical_sector - 33 + 2, next_address);
        put_in_data_area (next_sector, sectors_needed, total_size);
    }
}


/**
 * Function:  getFileCreationTime
 * --------------------
 * @brief get the creation time of the file.
 *        
 * @param filename: the name of the file.
 * @param year, month, day, hour, minute: buffers to store data.
 * 
 */
void getFileCreationTime(char *filename, char* year, char* month, char* day, char*hour, char*minute) {
    struct stat attr;
    stat(filename, &attr);

    int i =0;
    while(i<4){
        year[i] = ctime(&attr.st_mtime)[i+20];
        i++;
    }
    year[i] = '\0';

    i =0;
    while(i<3){
        month[i] = ctime(&attr.st_mtime)[i+4];
        i++;
    }
    month[i] = '\0';

    i =0;
    while(i<2){
        day[i] = ctime(&attr.st_mtime)[i+8];
        i++;
    }
    day[i] = '\0';

    i =0;
    while(i<2){
        hour[i] = ctime(&attr.st_mtime)[i+11];
        i++;
    }
    hour[i] = '\0';

    i =0;
    while(i<2){
        minute[i] = ctime(&attr.st_mtime)[i+14];
        i++;
    }
    minute[i] = '\0';
}


/**
 * Function:  process_date
 * --------------------
 * @brief process the create date of the file in order to later store it into a file entry in the disk.
 *        
 * @param year, month, day: the buffers containing the data as strings.
 * @param formatted_date: processed data contains the date and ready to be stored into a file entry.
 * 
 */
void process_date(char *year, char *month, char* day, uint16_t *formatted_date){
    int year_int = atoi(year);
    int month_int;
    if(strcmp(month, "Jan")==0){
        month_int = 1;
    }
    else if(strcmp(month, "Feb")==0){
        month_int = 2;
    }
    else if(strcmp(month, "Mar")==0){
        month_int = 3;
    }
    else if(strcmp(month, "Apr")==0){
        month_int = 4;
    }
    else if(strcmp(month, "May")==0){
        month_int = 5;
    }
    else if(strcmp(month, "Jun")==0){
        month_int = 6;
    }
    else if(strcmp(month, "Jul")==0){
        month_int = 7;
    }
    else if(strcmp(month, "Aug")==0){
        month_int = 8;
    }
    else if(strcmp(month, "Sep")==0){
        month_int = 9;
    }
    else if(strcmp(month, "Oct")==0){
        month_int = 10;
    }
    else if(strcmp(month, "Nov")==0){
        month_int = 11;
    }
    else{
        month_int = 12;
    }
    int day_int = atoi(day);
    int data_year = year_int - 1980;
    *formatted_date = (data_year << 9) + (month_int << 5) + day_int;
}


/**
 * Function:  process_time
 * --------------------
 * @brief process the create time of the file in order to later store it into a file entry in the disk.
 *        
 * @param hour, minute: the buffers containing the data as strings.
 * @param formatted_time: processed data contains the time and ready to be stored into a file entry.
 * 
 */
void process_time(char *hour, char *minute, uint16_t *formatted_time){
    int hour_int = atoi(hour);
    int minute_int = atoi(minute);
    *formatted_time = (hour_int << 11) + (minute_int << 5);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "usage: diskput <disk.img> [destination] <filename>, where [destination] is optional\n");
        exit(-1);
    }

    char* file_name;
    char* destination;
    if(argc == 3){
        file_name = argv[2];
        destination = "ROOT";
    }else{  // argc == 4
        file_name = argv[3];
        destination = argv[2];
    }

    if ((disk = fopen(argv[1], "r+")) == NULL) {
        fprintf(stderr, "Failed to open %s\n", argv[1]);
        exit(-1);
    }

    if ((file = fopen(file_name, "r")) == NULL) {
        printf("File not found. \n");
        fclose(disk);
        exit(-1);
    }

    // convert the filename to uppercase
    int i =0;
    while (file_name[i] != '\0'){
        file_name[i] = toupper(file_name[i]);
        i++;
    }

    /* Read boot sector */
    fseek(disk, 0x0, SEEK_SET);
    fread(&boot_sector, sizeof(boot_sector), 1, disk);

    int fat_mem_size;
    /* Allocate space for memory copy of the FAT */
    fat_size = boot_sector.total_sectors - 33 + 2;
    fat_mem_size = (fat_size & 0x01) ? (fat_size*3+1)/2 : fat_size*3/2;
    fat_table = emalloc(fat_mem_size);
    /* read a FAT copy */
    fseek(disk, 0x200, SEEK_SET);
    fread(fat_table, fat_mem_size, 1, disk);

    // get free size of the disk
    int freeBlocks = getFreeBlocks();
    if(freeBlocks == -1){
        printf("File system data hasn't loaded yet.\n");
        fclose(disk);
        fclose(file);
        exit(-1);
    }
    int free_disk_size = freeBlocks * 512;
    
    // get the size of the file
    fseek(file, 0, SEEK_END);
    int file_size = ftell(file);
    if(file_size>free_disk_size){
        printf("No enough free space in the disk image.\n");
        fclose(file);
        fclose(disk);
        exit(-1);
    }

    // get free entry in the destination directory
    int free_entry_address = -1;
    if(strcmp(destination, "ROOT")==0){
        get_free_entry_in_root_dir(disk, file_name, &free_entry_address);
    }else{
        get_free_sub_dir_entries(disk, 0, destination, &free_entry_address, file_name, "ROOT");
    }
    if(free_entry_address == -1){
        printf("The directory not found. \n");
        fclose(disk);
        fclose(file);
        exit(-1);
    }
    // overwrite the free entry in the destination directory with the new entry
    int free_sector = get_free_sector();
    entry_t new_entry;
    new_entry = fill_info_to_entry(file, file_name, free_sector);
    char year[5];
    char month[4];
    char day[3];
    char hour[3];
    char minute[3];
    // convert the filename to lowercase
    int j =0;
    while (file_name[j] != '\0'){
        file_name[j] = tolower(file_name[j]);
        j++;
    }
    getFileCreationTime(file_name, year, month, day, hour, minute);
    // convert the filename back to uppercase
    j =0;
    while (file_name[j] != '\0'){
        file_name[j] = toupper(file_name[j]);
        j++;
    }
    uint16_t formatted_date;
    process_date(year, month, day, &formatted_date);
    new_entry.create_date = formatted_date;
    new_entry.last_modified_date = formatted_date;
    uint16_t formatted_time;
    process_time(hour, minute, &formatted_time);
    new_entry.create_time = formatted_time;
    new_entry.last_modified_time= formatted_time;

    char entry_content[32];
    memcpy(&entry_content, (const unsigned char*) &new_entry, sizeof(entry_t));
    fseek(disk, free_entry_address, SEEK_SET);
    fwrite(&entry_content, sizeof(entry_t), 1, disk);
    
    int sectors_needed = file_size / 512 + (file_size % boot_sector.bytes_per_sector != 0);
    put_in_data_area (free_sector, sectors_needed, file_size);

    fclose(disk);
    fclose(file);
}