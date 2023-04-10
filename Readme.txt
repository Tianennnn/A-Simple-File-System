A Simple File System (SFS)

For this project, I implemented several utilities that can perform operations on a simple file system, FAT12, used by MS-DOS. Specifically, they are "diskinfo", "disklist", "diskget", "diskput".

How to compile:
There is a make file provided, so simply type "make" into the terminal to compile.

"diskinfo" is a program that displays information about the file system. The program can be invoked by: ./diskinfo <disk.img>
The output includes the following information:
OS Name:
Label of the disk:
Total size of the disk:
Free size of the disk:
The count of files in the disk including files in the root directory and files in all subdirectories:
Number of FAT copies:
Sectors per FAT:

"disklist" is a program that displays the contents of the root directory and all sub-directories in the file system. The program can be invoked by: ./disklist <disk.img>
In the output list, the first column will contain, "F" to indicate this entry is a file, or "D" to indicate this entry is a directory. For each file, the program will display the file_size in bytes, the file_name, and then the file creation date and creation time. 
Some part of the implementation referenced the instructor Ahmad Abdullah's code from the source file: listfile2.c.

"diskget" is a program that copies a file from the root directory of the file system to the current directory. The program can be invoked by:./diskget <disk.img> <filename>
If the specified file cannot be found in the root directory of the file system, the program will output the message "File not found" and exit. Else, the file <filename> should be copied to user's current (Linux) directory, and the user should be able to read the content of copied file.

"diskput" is a program that copies a file from the current directory into the specified directory (i.e., the root directory or a subdirectory) of the file system. The program can be invoked by: ./diskput <disk.img> [destination] <filename>, where optional [destination path] specifies the destination path within the file system starting from the root of the file system. If no [destination path] provided, then the file is copied to the root directory of the file system.
This program still has some bugs. The insertions of new files can be correctly reflected in the information displayed by calling "diskinfo", as well as the file list genersted by calling "disklist". However, when user try to call "diskget" to get the newly inserted file, the users cannot correctly retrieve the content of the files from the file system. 

