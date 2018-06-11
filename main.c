#include "config.h"

#ifdef HAVE_ERR
# include <err.h>
#endif
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "diff.h"

int 
main(int argc, char *argv[]) 
{
	size_t	 	 i;
	struct diff	 p;

	if (argc < 3) {
		fprintf(stderr, "usage: %s origin target\n", 
			getprogname());
		return EXIT_FAILURE;
	}

	diff(&p, argv[1], strlen(argv[1]), argv[2], strlen(argv[2]));

	for (i = 0; i < p.sessz; i++) {
		printf("%s %c\n",
			EDIT_ADD == p.ses[i].type ?
			"+" :
			EDIT_DELETE == p.ses[i].type ?
			"-" : "=",
			*p.ses[i].e);
	}

	free(p.ses);
	return 0;
}
