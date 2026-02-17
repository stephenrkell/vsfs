#include <stdlib.h>
#include <string.h>
#include <err.h>

/* In this file we actually define the table of commands and their argment
 * format strings. You are not expected to understand this. */
#define CMDLINE_FMT(ident, argstr) \
	extern const char ident ## _cmdline; \
	extern const char ident ## _cmdstr; \
	__asm__(".pushsection _cmdline_strings\n"\
            #ident "_cmdstring:\n"\
            ".asciz \"" #ident "\"\n"\
            #ident "_argstr:\n"\
            ".asciz \"" argstr "\"\n"\
            ".popsection\n"\
            ".pushsection _cmdline_cmds\n"\
            ".quad " #ident "_cmdstring\n"\
            ".quad " #ident "_argstr\n"\
            ".popsection\n")

typedef const char cmdpair[2];
extern cmdpair *__start__cmdline_cmds;
extern cmdpair *__stop__cmdline_cmds;

#include "vsfs.h"

int main(int argc, char **argv)
{
	debug_level = 11; // HACK
	debug_out = stderr; /* FIXME: allow config */
	if (argc < 2) errx(EXIT_FAILURE, "must name a backing file");
	vsfs_init(argv[1], TOTAL_BLOCKS * BLOCK_SIZE);
	/* To allow command-line interaction and test scripts,
	 * we read lines from stdin, to be parsed with fscanf/sscanf:
	 * first we do  "%s" to get 'command' and 'rest',
	 * and then will scan 'rest' according to a per-command format string:
	 *
	 * 
	 *
	 * */
	ssize_t nread;
	size_t bufsize = 0;
	char *lineptr = NULL;
	while (0 != (nread = getline(&lineptr, &bufsize, stdin)))
	{
		int nbytes;
		char cmd[11];
		int nfields = sscanf(lineptr, "%10s%n", (char*) &cmd, &nbytes);
		if (nfields >= 1)
		{
			/* We have a command in cmd */
			if (0 == strcmp(cmd, "link")) { warnx("FIXME: Do link"); }
			else if (0 == strcmp(cmd, "dumpfs")) { vsfs_dumpfs(); }
			else warnx("unknown command");
		}

		/* Ensure we always allocate a fresh buffer. See getline(3). */
		bufsize = 0; free(lineptr);
	}
	if (lineptr) free(lineptr);
	return 0;
}
