/*
 * aquosctl - Control Sharp Aquos televisions via RS-232 interface.
 *
 * Jason White <jdwhite@menelos.com>
 *
 * RS-232C port specifications, command format, parameter specification,
 * response code format, and command table are referenced from the Sharp 
 * Aquos operation manual for the LC-42/46/52D64U, revision 12/16/05.
 *
 * "NEWER_PROTOCOL" data from the Sharp Aquos operation smanual for the 
 * LC-80LE844U/LC-70LE847U/LC-60LE847U/LC-70LE745U/LC-60LE745U, revsion 
 * 12/17/10.
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
#define CMD_3D       24
#define CMD_BUTTON   25

#ifdef NEWER_PROTOCOL
#define CMD_TABLE_VERSION "12/17/10"
#else
#define CMD_TABLE_VERSION "12/16/05"
#endif

static struct lookuptab {
	char *cmd; int opcode;
		char *args;
		char *desc;
} cmdtab[] = {
	{"poenable", CMD_POENABLE,
#ifdef NEWER_PROTOCOL
		"{ on | on-ip | off }",
#else
		"{ on | off }",
#endif
		"Enable/Disable power on command."
	},
	{"power", CMD_POWER,
		"{ on | off }",
		"Turn TV on/off."
	},
	{"input", CMD_INPUT,
#ifdef NEWER_PROTOCOL
		"[ tv | 1 - 8 ]",
		"Select TV, INPUT1-8; blank to toggle."
#else
		"[ tv | 1 - 7 ]",
		"Select TV, INPUT1-7; blank to toggle."
#endif
	},
	{"avmode", CMD_AVMODE,
#ifdef NEWER_PROTOCOL
		"[standard|movie|game|user|dyn-fixed|dyn|pc|xvycc|standard-3d|movie-3d|game-3d|auto]",
#else
		"[standard|movie|game|user|dyn-fixed|dyn|pc|xvycc]",
#endif
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
#ifdef NEWER_PROTOCOL
		"{sidebar|sstretch|zoom|stretch|normal|zoom-pc|stretch-pc|dotbydot|full}",
#else
		"{sidebar|sstretch|zoom|stretch|normal|zoom-pc|stretch-pc|dotbydot|full|auto|original}",
#endif
		"View modes (vary depending on input signal type -- see manual)."
	},
	{"mute", CMD_MUTE,
		"[ on | off ]",
		"Mute on/off; blank to toggle."
	},
	{"surround", CMD_SURROUND,
#ifdef NEWER_PROTOCOL
		"[ on | off ]",
#else
		"[ normal | off | 3d-hall | 3d-movie | 3d-standard | 3d-stadium ]",
#endif
		"Surround mode; blank to toggle."
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
#ifdef NEWER_PROTOCOL
	{"3d", CMD_3D,
		"{ off | 2d3d | sbs | tab | 3d2d-sbs | 3d2d-tab | 3d-auto | 2d-auto }",
		"3D mode selection."
	},
	{"button", CMD_BUTTON,
		"{ button on remote }",
		"Simulate remote control button press."
	},
#endif
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
				sprintf(param, "%-4s", "0"); /* Disable power on cmd */
			}
			else if (strcmp(arg, "on") == 0) {
				sprintf(param, "%-4s", "1"); /* Enable power on cmd */
			}
#ifdef NEWER_PROTOCOL
			else if (strcmp(arg, "on-ip") == 0) {
				sprintf(param, "%-4s", "2"); /* Enable power on cmd */
			}
#endif /* NEWER_PROTOCOL */
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
#ifdef NEWER_PROTOCOL
			else if ((atoi(arg) >= 1 && atoi(arg) <= 8) &&
#else
			else if ((atoi(arg) >= 1 && atoi(arg) <= 7) &&
#endif
			         (strcmp(arg2, "") == 0)) { /* input select */
				sprintf(param, "%-4s", arg);
				sendcommand("IAVD", param);
			}
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
#ifdef NEWER_PROTOCOL
			else if (strcmp(arg, "standard-3d") == 0) {
				sprintf(param, "%-4s", "14");
			}
			else if (strcmp(arg, "movie-3d") == 0) {
				sprintf(param, "%-4s", "15");
			}
			else if (strcmp(arg, "game-3d") == 0) {
				sprintf(param, "%-4s", "16");
			}
			else if (strcmp(arg, "auto") == 0) {
				sprintf(param, "%-4s", "100");
			}
#endif
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
#ifdef NEWER_PROTOCOL
			else if (strcmp(arg, "auto") == 0) {
				sprintf(param, "%-4s", "10");
			}
			else if (strcmp(arg, "original") == 0) {
				sprintf(param, "%-4s", "11");
			}
#endif
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
#ifdef NEWER_PROTOCOL
			else if (strcmp(arg, "normal") == 0) {
				sprintf(param, "%-4s", "1");
			}
#else
			else if (strcmp(arg, "on") == 0) {
				sprintf(param, "%-4s", "1");
			}
#endif
			else if (strcmp(arg, "off") == 0) {
				sprintf(param, "%-4s", "2");
			}
#ifdef NEWER_PROTOCOL
			else if (strcmp(arg, "3d-hall") == 0) {
				sprintf(param, "%-4s", "4");
			}
			else if (strcmp(arg, "3d-movie") == 0) {
				sprintf(param, "%-4s", "5");
			}
			else if (strcmp(arg, "3d-standard") == 0) {
				sprintf(param, "%-4s", "6");
			}
			else if (strcmp(arg, "3d-stadium") == 0) {
				sprintf(param, "%-4s", "7");
			}
#endif
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

#ifdef NEWER_PROTOCOL
		case CMD_3D:
			if (strcmp(arg, "off") == 0) {
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "2d3d") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else if (strcmp(arg, "sbs") == 0) {
				sprintf(param, "%-4s", "2");
			}
			else if (strcmp(arg, "tab") == 0) {
				sprintf(param, "%-4s", "3");
			}
			else if (strcmp(arg, "3d2d-sbs") == 0) {
				sprintf(param, "%-4s", "4");
			}
			else if (strcmp(arg, "3d2d-tab") == 0) {
				sprintf(param, "%-4s", "5");
			}
			else if (strcmp(arg, "3d-auto") == 0) {
				sprintf(param, "%-4s", "6");
			}
			else if (strcmp(arg, "2d-auto") == 0) {
				sprintf(param, "%-4s", "7");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("TDCH", param);
			break;

		case CMD_BUTTON:
			if (strcmp(arg, "0") == 0) {
				sprintf(param, "%-4s", "0");
			}
			else if (strcmp(arg, "1") == 0) {
				sprintf(param, "%-4s", "1");
			}
			else if (strcmp(arg, "2") == 0) {
				sprintf(param, "%-4s", "2");
			}
			else if (strcmp(arg, "3") == 0) {
				sprintf(param, "%-4s", "3");
			}
			else if (strcmp(arg, "4") == 0) {
				sprintf(param, "%-4s", "4");
			}
			else if (strcmp(arg, "5") == 0) {
				sprintf(param, "%-4s", "5");
			}
			else if (strcmp(arg, "6") == 0) {
				sprintf(param, "%-4s", "6");
			}
			else if (strcmp(arg, "7") == 0) {
				sprintf(param, "%-4s", "7");
			}
			else if (strcmp(arg, "8") == 0) {
				sprintf(param, "%-4s", "8");
			}
			else if (strcmp(arg, "9") == 0) {
				sprintf(param, "%-4s", "9");
			}
			else if (strcmp(arg, ".") == 0) {
				sprintf(param, "%-4s", "10");
			}
			else if (strcmp(arg, "ent") == 0 ||
			         strcmp(arg, "enter") == 0) {
				sprintf(param, "%-4s", "11");
			}
			else if (strcmp(arg, "power") == 0) {
				sprintf(param, "%-4s", "12");
			}
			else if (strcmp(arg, "display") == 0) {
				sprintf(param, "%-4s", "13");
			}
			else if (strcmp(arg, "power-source") == 0) {
				sprintf(param, "%-4s", "14");
			}
			else if (strcmp(arg, "rew") == 0) { /* << */
				sprintf(param, "%-4s", "15");
			}
			else if (strcmp(arg, "play") == 0) {
				sprintf(param, "%-4s", "16");
			}
			else if (strcmp(arg, "ff") == 0) { /* >> */
				sprintf(param, "%-4s", "17");
			}
			else if (strcmp(arg, "pause") == 0) { /* || */
				sprintf(param, "%-4s", "18");
			}
			else if (strcmp(arg, "prev") == 0) { /* |<< */
				sprintf(param, "%-4s", "19");
			}
			else if (strcmp(arg, "stop") == 0) {
				sprintf(param, "%-4s", "20");
			}
			else if (strcmp(arg, "next") == 0) { /* >>| */
				sprintf(param, "%-4s", "21");
			}
			else if (strcmp(arg, "rec") == 0) {
				sprintf(param, "%-4s", "22");
			}
			else if (strcmp(arg, "option") == 0) {
				sprintf(param, "%-4s", "23");
			}
			else if (strcmp(arg, "sleep") == 0) {
				sprintf(param, "%-4s", "24");
			}
			else if (strcmp(arg, "cc") == 0) {
				sprintf(param, "%-4s", "27");
			}
			else if (strcmp(arg, "avmode") == 0) {
				sprintf(param, "%-4s", "28");
			}
			else if (strcmp(arg, "viewmode") == 0) {
				sprintf(param, "%-4s", "29");
			}
			else if (strcmp(arg, "flashback") == 0) {
				sprintf(param, "%-4s", "30");
			}
			else if (strcmp(arg, "mute") == 0) {
				sprintf(param, "%-4s", "31");
			}
			else if (strcmp(arg, "vol-") == 0 ||
			         strcmp(arg, "voldn") == 0) {
				sprintf(param, "%-4s", "32");
			}
			else if (strcmp(arg, "vol+") == 0 ||
			         strcmp(arg, "volup") == 0) {
				sprintf(param, "%-4s", "33");
			}
			else if (strcmp(arg, "chup") == 0) {
				sprintf(param, "%-4s", "34");
			}
			else if (strcmp(arg, "chdn") == 0) {
				sprintf(param, "%-4s", "35");
			}
			else if (strcmp(arg, "input") == 0) {
				sprintf(param, "%-4s", "36");
			}
			else if (strcmp(arg, "menu") == 0) {
				sprintf(param, "%-4s", "38");
			}
			else if (strcmp(arg, "startcenter") == 0) {
				sprintf(param, "%-4s", "39");
			}
			else if (strcmp(arg, "enter") == 0) {
				sprintf(param, "%-4s", "40");
			}
			else if (strcmp(arg, "up") == 0) {
				sprintf(param, "%-4s", "41");
			}
			else if (strcmp(arg, "down") == 0) {
				sprintf(param, "%-4s", "42");
			}
			else if (strcmp(arg, "left") == 0) {
				sprintf(param, "%-4s", "43");
			}
			else if (strcmp(arg, "right") == 0) {
				sprintf(param, "%-4s", "44");
			}
			else if (strcmp(arg, "return") == 0) {
				sprintf(param, "%-4s", "45");
			}
			else if (strcmp(arg, "exit") == 0) {
				sprintf(param, "%-4s", "46");
			}
			else if (strcmp(arg, "fav") == 0 ||
			         strcmp(arg, "favorite") == 0 ||
			         strcmp(arg, "favoritech") == 0) {
				sprintf(param, "%-4s", "47");
			}
			else if (strcmp(arg, "3d-surround") == 0) {
				sprintf(param, "%-4s", "48");
			}
			else if (strcmp(arg, "audio") == 0) {
				sprintf(param, "%-4s", "49");
			}
			else if (strcmp(arg, "a") == 0 ||
			         strcmp(arg, "red") == 0) {
				sprintf(param, "%-4s", "50");
			}
			else if (strcmp(arg, "b") == 0 ||
			         strcmp(arg, "green") == 0) {
				sprintf(param, "%-4s", "51");
			}
			else if (strcmp(arg, "c") == 0 ||
			         strcmp(arg, "blue") == 0) {
				sprintf(param, "%-4s", "52");
			}
			else if (strcmp(arg, "d") == 0 ||
			         strcmp(arg, "yellow") == 0) {
				sprintf(param, "%-4s", "53");
			}
			else if (strcmp(arg, "freeze") == 0) {
				sprintf(param, "%-4s", "54");
			}
			else if (strcmp(arg, "favapp1") == 0) {
				sprintf(param, "%-4s", "55");
			}
			else if (strcmp(arg, "favapp2") == 0) {
				sprintf(param, "%-4s", "56");
			}
			else if (strcmp(arg, "favapp3") == 0) {
				sprintf(param, "%-4s", "57");
			}
			else if (strcmp(arg, "3d") == 0) {
				sprintf(param, "%-4s", "58");
			}
			else if (strcmp(arg, "netflix") == 0) {
				sprintf(param, "%-4s", "59");
			}
			else {
				fprintf(stderr,
					"%s: Invalid parameter \"%s\" for command %s.\n",
					progname, arg, oparg
				);

				return(EXIT_FAILURE);
			}

			sendcommand("RCKY", param);
			break;
#endif
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
			"aquosctl (command protocol revision %s)\n"
	        "usage: %s [ -h | -n | -p {port} | -v ] {command} [arg]\n",
			CMD_TABLE_VERSION, progname
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
