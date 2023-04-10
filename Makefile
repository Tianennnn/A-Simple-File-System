all: diskinfo disklist diskget diskput
diskinfo: diskinfo.c
		gcc -o diskinfo diskinfo.c emalloc.c

disklist: disklist.c
		gcc -o disklist disklist.c emalloc.c

diskget: diskget.c
		gcc -o diskget diskget.c emalloc.c

diskput: diskput.c
		gcc -o diskput diskput.c emalloc.c

.PHONY: all