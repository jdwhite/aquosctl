/*
 * aquosctl - Control Sharp Aquos televisions via RS-232 interface.
 *
 * Jason White <jdwhite@menelos.com>
 *
 * RS-232C port specifications, command format, parameter specification,
 * response code format, and command table are referenced from the Sharp 
 * Aquos operation manual for the LC-42/46/52D64U, revision 12/16/05.
 * 
 * Note: Direct Channel (digital) functionality has not been tested and 
 *       formatting of channel numbers may need tweaking.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <signal.h>

/* Linux com0 is /dev/ttyS0. */
#define	DEFAULT_PORT "/dev/ttyS0"

#define CMD_NONE      0
#define CMD_POENABLE  1
#define CMD_POWER     2
#define CMD_INPUT     3
#define CMD_AVMODE    6
#define CMD_VOLUME    7
#define CMD_HPOS      8
#define CMD_VPOS      9
#define CMD_CLOCK    10
#define CMD_PHASE    11
#define CMD_VIEWMODE 12
#define CMD_MUTE     13
#define CMD_SURROUND 14
#define CMD_AUDIOSEL 15
#define CMD_SLEEP    16
#define CMD_ACHAN    17
#define CMD_DCHAN    18
#define CMD_DCABL1   19
#define CMD_DCABL2   20
#define CMD_CHUP     21
#define CMD_CHDN     22
#define CMD_CC       23

/* Aquos Command Table, 12/16/05 */
static struct lookuptab {
	char *cmd; int opcode;
		char *args;
		char *desc;
} cmdtab[] = {
	{"poenable", CMD_POENABLE,
		"{ on | off }",
		"Enable/Disable power on command."
	},
	{"power", CMD_POWER,
		"{ on | off }",
		"Turn TV on/off."
	},
	{"input", CMD_INPUT,
		"[ tv | 1 - 7 ]",
		"Select TV, INPUT1-7; blank to toggle."
	},
	{"avmode", CMD_AVMODE,
		"[standard|movie|game|user|dyn-fixed|dyn|pc|xvycc]",
		"AV mode selection; blank to toggle."
	},
	{"vol",	CMD_VOLUME,
		"{ 0 - 60 }",
		"Set volume (0-60)."
	},
	{"hpos", CMD_HPOS,
		"<varies depending on View Mode or signal type>",
		"Horizontal Position. Ranges are on the position setting screen."
	},
	{"vpos", CMD_VPOS,
		"<varies depending on View Mode or signal type>",
		"Vertical Position. Ranges are on the position setting screen."
	},
	{"clock", CMD_CLOCK,
		"{ 0 - 180 }",
		"Only in PC mode."
	},
	{"phase", CMD_PHASE,
		"{ 1 - 40 }",
		"Only in PC mode."
	},
	{"viewmode", CMD_VIEWMODE,
		"{sidebar|sstretch|zoom|stretch|normal|zoom-pc|stretch-pc|dotbydot|full}",
		"View modes (vary depending on input signal type -- see manual)."
	},
	{"mute", CMD_MUTE,
		"[ on | off ]",
		"Mute on/off; blank to toggle."
	},
	{"surround", CMD_SURROUND,
		"[ on | off ]",
		"Surround on/off; blank to toggle."
	},
	{"audiosel", CMD_AUDIOSEL,
		"<none>",
		"Audio selection toggle."
	},
	{"sleep", CMD_SLEEP,
		"{ off or 0 | 30 | 60 | 90 | 120 }",
		"Sleep timer off or 30/60/90/120 minutes."
	},
	{"achan", CMD_ACHAN,
		"{ 1 - 135 }",
		"Analog channel selection. Over-the-air: 2-69, Cable: 1-135."
	},
	{"dchan", CMD_DCHAN,
		"{ xx.yy } or { xx } (xx=channel 1-99, yy=subchannel 1-99)",
		"Digital over-the-air channel selection."
	},
	{"dcabl1", CMD_DCABL1,
		"{ xxx.yyy } or { xxx } (xxx=major ch. 1-999, yyy=minor ch. 0-999)",
		"Digital cable (type one)."
	},
	{"dcabl2", CMD_DCABL2,
		"{ 0 - 16383 }",
		"Digital cable (type two), channels 0-16383."
	},
	{"chup", CMD_CHUP,
		"<none>",
		"Channel up. Will switch to TV input if not already selected."
	},
	{"chdn", CMD_CHDN,
		"<none>",
		"Channel down. Will switch to TV input if not already selected."
	},
	{"cc", CMD_CC,
		"<none>",
		"Closed Caption toggle."
	},
};

int fd;
int nosend = 0;
int verbose = 0;

/* Prototypes */
void openport(char []);
int  sendcommand(char [], char []);
int  checkcmd(char []);
void usage(char []);
void leave(int);

int
main (
int  argc,
	char **argv
)
{
	extern char *optarg;
	extern int  optind;
	int         ch = 0,
	            chan = 0,
	            subchan = 0;
	char        cmd[5] = "",
	            param[5] = "",
	            *progname = argv[0],
	            oparg[16] = "",
	            arg[16] = "",
	            arg2[16] = "",
	            port[32] = DEFAULT_PORT;

	(void) signal(SIGALRM, leave);

	if (argc == 1) {
		usage(progname);
	}

	while ((ch = getopt(argc, argv, "vhnp:")) != -1) {
		switch(ch) {
			case 'n':
				nosend = 1; /* for debugging protocol formatting */
				break;
			case 'v':
				verbose = 1;
				break;
			case 'p':
				if (strcmp(optarg, "") == 0) {
					fprintf(stderr,"no port specified\n");
					usage(progname);
				}
				strcpy(port, optarg);
				break;

			case 'h':
			default:
				usage(progname);
		}
	}
	argc -= optind;
	argv += optind;

    if (verbose == 1) printf("port=%s\n", port);
/*
	printf("argc=%d\n", argc);
	printf("argv[0]=%s\n", argv[0]);
	printf("argv[1]=%s\n", argv[1]);
*/

	if (argc >= 1) strcpy(oparg, argv[0]);
	if (argc >= 2) strcpy(arg, argv[1]);
	if (argc >= 3) strcpy(arg2, argv[2]);

	if (nosend == 0) openport(port);

	switch(checkcmd(oparg)) {
		case CMD_NONE:
			fprintf(stderr, "%s: bad command '%s'\n", progname, oparg);
			return(EXIT_FAILURE);
			break;

		case CMD_POENABLE:
			if (strcmp(arg, "off") == 0) {
				sprintf(param, "%-4s", "0"); /* Enable power on cmd */
			}
			else if (strcmp(arg, "on") == 0) {
				sprintf(param, "%-4s", "1"); /* Disable power on cmd */
			}
			else {
				fprintf(stderr, 
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("RSPW", param);
			break;

		case CMD_POWER:
			if (strcmp(arg, "off") == 0) {
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "on") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("POWR", param);
			break;

		case CMD_INPUT:
			if (strcmp(arg, "") == 0) {		/* toggle video */
				sprintf(param, "%-4s", "0");
				sendcommand("ITGD", param);
			}
			else if (strcmp(arg, "tv") == 0) { /* select TV */
				sprintf(param, "%-4s", "0");
				sendcommand("ITVD", param);
			}
			else if ((atoi(arg) >= 1 && atoi(arg) <= 7) &&
			         (strcmp(arg2, "") == 0)) { /* input select */
				sprintf(param, "%-4s", arg);
				sendcommand("IAVD", param);
			}

/******* This seems wrong according to the 2005 spec. ******************/
			else if ((atoi(arg) >= 1 && atoi(arg) <= 7) &&
			         (strcmp(arg2, "auto") == 0)) { /* video select */
				sprintf(cmd, "INP%s", arg);
				sprintf(param, "%-4s", "0");

				sendcommand(cmd, param);
			}
			else if ((atoi(arg) >= 1 && atoi(arg) <= 7) &&
			         (strcmp(arg2, "video") == 0)) { /* video select */
				sprintf(cmd, "INP%s", arg);
				sprintf(param, "%-4s", "1");

				sendcommand(cmd, param);
			}
			else if ((atoi(arg) >= 1 && atoi(arg) <= 7) &&
			         (strcmp(arg2, "component") == 0)) { /* video select */
				sprintf(cmd, "INP%s", arg);
				sprintf(param, "%-4s", "2");

				sendcommand(cmd, param);
			}
/********************************************************************/
			else {
				fprintf(stderr,
					"%s: Invalid parameter(s) for command %s.\n",
					progname, oparg
				);

				return(EXIT_FAILURE);
			}

			break;

		case CMD_AVMODE:
			if (strcmp(arg, "") == 0) { /* toggle */
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "standard") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else if (strcmp(arg, "movie") == 0) {
				sprintf(param, "%-4s", "2");
			}
			else if (strcmp(arg, "game") == 0) {
				sprintf(param, "%-4s", "3");
			}
			else if (strcmp(arg, "user") == 0) {
				sprintf(param, "%-4s", "4");
			}
			else if (strcmp(arg, "dyn-fixed") == 0) {
				sprintf(param, "%-4s", "5");
			}
			else if (strcmp(arg, "dyn") == 0) {
				sprintf(param, "%-4s", "6");
			}
			else if (strcmp(arg, "pc") == 0) {
				sprintf(param, "%-4s", "7");
			}
			else if (strcmp(arg, "xvycc") == 0) {
				sprintf(param, "%-4s", "8");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("AVMD", param);
			break;

		case CMD_VOLUME:
			if ((strcmp(arg, "") != 0) &&
				(atoi(arg) >= 0 && atoi(arg) <= 60)) {
				sprintf(param, "%-4s", arg);
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("VOLM", param);
			break;

		case CMD_HPOS:
			/* Range depends on view mode and type, so can't
			   range check beyond 0-999. */
			if ((strcmp(arg, "") != 0) &&
				(atoi(arg) >= 0 && atoi(arg) <= 999)) {
				sprintf(param, "%-4s", arg);
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("HPOS", param);
			break;

		case CMD_VPOS:
			/* Range depends on view mode and type, so can't 
			   range check beyond 0-999. */
			if ((strcmp(arg, "") != 0) &&
				(atoi(arg) >= 0 && atoi(arg) <= 999)) {
				sprintf(param, "%-4s", arg);
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("VPOS", param);
			break;

		case CMD_CLOCK:
			if ((strcmp(arg, "") != 0) &&
			    (atoi(arg) >= 0 && atoi(arg) <= 180)) {
				sprintf(param, "%-4s", arg);
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("CLCK", param);
			break;

		case CMD_PHASE:
			if ((strcmp(arg, "") != 0) &&
			    (atoi(arg) >= 0 && atoi(arg) <= 40)) {
				sprintf(param, "%-4s", arg);
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("PHSE", param);
			break;

		case CMD_VIEWMODE:
			if (strcmp(arg, "") == 0) { /* toggle */
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "sidebar") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else if (strcmp(arg, "sstretch") == 0) {
				sprintf(param, "%-4s", "2");
			}
			else if (strcmp(arg, "zoom") == 0) {
				sprintf(param, "%-4s", "3");
			}
			else if (strcmp(arg, "stretch") == 0) {
				sprintf(param, "%-4s", "4");
			}
			else if (strcmp(arg, "normal") == 0) {
				sprintf(param, "%-4s", "5");
			}
			else if (strcmp(arg, "zoom-pc") == 0) {
				sprintf(param, "%-4s", "6");
			}
			else if (strcmp(arg, "stretch-pc") == 0) {
				sprintf(param, "%-4s", "7");
			}
			else if (strcmp(arg, "dotbydot") == 0) {
				sprintf(param, "%-4s", "8");
			}
			else if (strcmp(arg, "full") == 0) {
				sprintf(param, "%-4s", "9");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("WIDE", param);
			break;

		case CMD_MUTE:
			if (strcmp(arg, "") == 0) { /* toggle */
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "on") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else if (strcmp(arg, "off") == 0) {
				sprintf(param, "%-4s", "2");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("MUTE", param);
			break;

		case CMD_SURROUND:
			if (strcmp(arg, "") == 0) { /* toggle */
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "on") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else if (strcmp(arg, "off") == 0) {
				sprintf(param, "%-4s", "2");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("ACSU", param);
			break;

		case CMD_AUDIOSEL:
			sprintf(param, "%-4s", "0"); /* toggle */
			sendcommand("ACHA", param);
			break;

		case CMD_SLEEP:
			if (strcmp(arg, "off") == 0) {
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "0") == 0) {
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "30") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else if (strcmp(arg, "60") == 0) {
				sprintf(param, "%-4s", "2");
			}
			else if (strcmp(arg, "90") == 0) {
				sprintf(param, "%-4s", "3");
			}
			else if (strcmp(arg, "120") == 0) {
				sprintf(param, "%-4s", "4");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("OFTM", param);
			break;

		case CMD_ACHAN:
			if (atoi(arg) >= 1 && atoi(arg) <= 135) {
				sprintf(param, "%-4s", arg);
			} else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("DCCH", param);
			break;

		case CMD_DCHAN:
			/* This will catch channel formats "xxyy" and "xx.yy". */
			if (sscanf(arg, "%d.%d", &chan, &subchan) > 0) {
				sprintf(param, "%02d%02d", chan, subchan);
			} else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("DA2P", param);
			break;

		case CMD_DCABL1:
			/* This will catch channel formats "xxx" and "xxx.yyy". */
			if ((sscanf(arg, "%d.%d", &chan, &subchan) > 0) &&
		        (chan <= 999 && subchan <= 999)) {
			} else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sprintf(param, "%03d ", chan);
			sendcommand("DC2U", param);
			sprintf(param, "%03d ", subchan);
			sendcommand("DC2L", param);
			break;

		case CMD_DCABL2:
			if (atoi(arg) >= 0 && atoi(arg) <= 9999) {
				sprintf(param, "%04d", atoi(arg));
				sendcommand("DC10", param);
			}
			else if (atoi(arg) > 9999 && atoi(arg) <= 16383) {
				sprintf(param, "%04d", atoi(arg) - 10000);
				sendcommand("DC11", param);
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			break;

		case CMD_CHUP:
			sprintf(param, "%-4s", "0");
			sendcommand("CHUP", param);
			break;

		case CMD_CHDN:
			sprintf(param, "%-4s", "0");
			sendcommand("CHDW", param);
			break;

		case CMD_CC:
			sprintf(param, "%-4s", "0"); /* toggle */
			sendcommand("CLCP", param);
			break;
	}

	close(fd);

	return(EXIT_SUCCESS);
}

void
openport(
	char *port
)
{
	struct termios options;

	fd = open(port, O_RDWR | O_NOCTTY | O_NDELAY);
	if (fd == -1) {
		fprintf(stderr, "openport(%s): %s\n", port, strerror(errno));
		exit(EXIT_FAILURE);
	}

	fcntl(fd, F_SETFL, 0); /* Make reads return immediately. */

	tcgetattr(fd, &options); /* Get current port options. */

	/* Set the baud rates to 9600,8,N,1. */
	cfsetispeed(&options, B9600);
	cfsetospeed(&options, B9600);
	options.c_cflag &= ~PARENB;
	options.c_cflag &= ~CSTOPB;
	options.c_cflag &= ~CSIZE;
	options.c_cflag |= CS8;

	options.c_cflag &= ~CRTSCTS; /* No hardware flow control. */

	options.c_cflag |= (CLOCAL | CREAD); /* Enable receiver, set local mode. */
	options.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* Raw input. */

	options.c_iflag &= ~(IXON | IXOFF | IXANY); /* No software flow control. */
	options.c_oflag &= ~OPOST; /* Raw output. */

	tcsetattr(fd, TCSANOW, &options); /* Set options for the new port. */

	return;
}

int
sendcommand(
	char *command,
	char *parameter
)
{
	int  nbytes;
	char buffer[255];
	char *buffptr;

	if (verbose == 1 || nosend == 1) {
		printf("command='%s', parameter='%s'\n", command, parameter);
	}

	if (nosend == 1) return(EXIT_SUCCESS);

	write(fd, command, strlen(command));
	write(fd, parameter, strlen(parameter));
	write(fd, "\r", 1);

	/* Some commands (CHUP, CHDW) don't issue a response, so timeout after
	 * one second and just exit. This may cause problems with 
	 * multi-command functions such as Digital Cable tuning options 
	 * since the first sequence may succeeed on the TV side, but not be 
	 * reported at 'OK' by the TV, thereby causing the second half of 
	 * the tuning command not to be sent, but this is just a hypothesis.
	 */

	alarm(1);

	buffptr = buffer;
	while ((nbytes = read(fd, buffptr, buffer+sizeof(buffer)-buffptr-1)) > 0) {
		buffptr += nbytes;
		if (buffptr[-1] == '\n' || buffptr[-1] == '\r')
			break;
	}

	/* NULL terminate and chop cr/nl. */
	buffptr[-1] = '\0';

	if (strncmp(buffer, "OK", 2) == 0) {
		if (verbose == 1) puts("Success.");
		return(EXIT_SUCCESS);
	}
	else if (strncmp(buffer, "ERR", 3) == 0) {
		fprintf(stderr, "Error: command/param '%s%s'\n", command, parameter);
		return(EXIT_FAILURE);
	}
	else {
		fprintf(stderr,
			"Error: unexpected response '%s' to command/param '%s%s'\n",
			buffer, command, parameter
		);
		return(EXIT_FAILURE);
	}
}

int
checkcmd(
	char	*string
)
{
	int i;
	for(i = 0; i < sizeof(cmdtab) / sizeof(cmdtab[0]); i++)
	if (strcmp(cmdtab[i].cmd, string) == 0) {
		return cmdtab[i].opcode;
	}

	return CMD_NONE;
}

void
leave(
	int sig
)
{
	puts("No response.");
	close(fd);

	exit(EXIT_FAILURE);
}

void
usage(
	char	*progname
)
{
	int i;
	fprintf(stderr,
	        "usage: %s [ -h | -n | -p {port} | -v ] {command} [arg]\n",
	        progname
	);
	fprintf(stderr,
		"\t-h\tHelp\n"
		"\t-n\tShow commands being sent, but don't send them (No-send).\n"
		"\t-p\tSerial Port to use (default is %s).\n"
		"\t-v\tVerbose mode.\n\n"
		"command    args\n--------------------",
		DEFAULT_PORT
	);
	for(i = 0; i < sizeof(cmdtab) / sizeof(cmdtab[0]); i++) {
		fprintf(stderr,
			"\n%-10s %s\n           %s\n", 
			cmdtab[i].cmd, cmdtab[i].args, cmdtab[i].desc
		);
	}

	exit(EXIT_FAILURE);
}
