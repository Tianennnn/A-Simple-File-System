# A Simple File System (SFS)

For this project, I implemented several utilities that can perform operations on a simple file system, FAT12, used by MS-DOS. Specifically, they are "diskinfo", "disklist", "diskget" and "diskput".

<b> - *diskinfo*</b> is a program that displays information about the file system. The program can be invoked by: <br>
```
./diskinfo <disk.img>
```
The output includes the following information: <br>
OS Name: <br>
Label of the disk: <br>
Total size of the disk: <br>
Free size of the disk: <br>
The count of files in the disk including files in the root directory and files in all subdirectories: <br>
Number of FAT copies: <br>
Sectors per FAT:<br>

<br>

<b> - *disklist*</b> is a program that displays the contents of the root directory and all sub-directories in the file system. The program can be invoked by: 
```
./disklist <disk.img>
```
In the output list, the first column will contain, "F" to indicate this entry is a file, or "D" to indicate this entry is a directory. For each file,
the program will display the file_size in bytes, the file_name, and then the file creation date and creation time.<br>

<br> 

<b> - *diskget*</b> is a program that copies a file from the root directory of the file system to the current directory. The program can be invoked by:
```
./diskget <disk.img> <filename>
```
If the specified file cannot be found in the root directory of the file system, the program will output the message "File not found" and exit. 
Else, the file <filename> should be copied to user's current (Linux) directory, and the user should be able to read the content of copied file. <br>
  
<br>
  
<b> - *diskput*</b> is a program that copies a file from the current directory into the specified directory (i.e., the root directory or a subdirectory) of the file system. 
The program can be invoked by: 
```
./diskput <disk.img> [destination] <filename>
```
where optional [destination path] specifies the destination path within the file system starting from the root of the file system. If no [destination path] provided, then the file is copied to the root directory of the file system.

# How to compile:
There is a make file provided, so simply type "make" into the terminal to compile.


