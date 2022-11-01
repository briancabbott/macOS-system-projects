#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "different.h"
#include "gripes.h"
#include "man-config.h"
#include "util.h"

static struct filelist {
    char *pathname;
    struct filelist *next;
} cat_list, man_list;

static int
is_different(const char *file, struct filelist *p) {
    char *command;
    const char *cmp = getval("CMP");
    int ret;

    if (cmp) {
	while (p->next) {
	    command = my_xsprintf("%s %S %S\n", cmp, file, p->pathname);
	    ret = do_system_command (command, 1);
	    if (ret == 0) {
		gripe(IDENTICAL, file, p->pathname);
		return 0;
	    }
	    p = p->next;
	}
	p->next = (struct filelist *) my_malloc(sizeof(struct filelist));
	p->pathname = my_strdup(file);
	p->next->next = 0;
    }
    return 1;
}

static int
free_filelist (struct filelist *list){
struct filelist *current, *next;

 current = list; 
 if (current != list)
    do {
        next = current->next;
        if (current != list)
            free(current);
        current = next;
    } while (current->next != NULL);

 list->next = NULL;  
 
 return 0; 
}

int
different_cat_file (const char *file) {
    return is_different (file, &cat_list);
}

int
different_man_file (const char *file) {
    return is_different (file, &man_list);
}

int 
free_catman_filelists (void){

    free_filelist(&man_list);
    free_filelist(&cat_list);
return 0;
}
