/*
 * main.c
 */

#define _DEFAULT_SOURCE

#include "ch.h"
#include "hal.h"
#include "ch_test.h"

#include <stdlib.h>
#include <string.h>

#include "usbcfg.h"

/*
 * LED blinker thread.
 */
static THD_WORKING_AREA(waThread1, 128);
static THD_FUNCTION(Thread1, arg)
{
	(void)arg;
	chRegSetThreadName("blinker");
	while (true) {
		systime_t time;

		time = serusbcfg.usbp->state == USB_ACTIVE ? 150 : 500;
		palClearPad(TEENSY_PIN13_IOPORT, TEENSY_PIN13);
		chThdSleepMilliseconds(time);
		palSetPad(TEENSY_PIN13_IOPORT, TEENSY_PIN13);
		chThdSleepMilliseconds(time);
	}
}

static struct pwm_desc
{
	ioportid_t port;
	uint8_t    pad;
	uint8_t    ch;
	uint16_t   pos;
} pwm_channels[3] = {
	/* TEENSY_21, PTD6, FMT0_CH6, RED */
	{ TEENSY_PIN21_IOPORT, TEENSY_PIN21, 6,   0 },
	/* TEENSY_22, PTC1, FMT0_CH0, GREEN */
	{ TEENSY_PIN22_IOPORT, TEENSY_PIN22, 0,  85 },
	/* TEENSY_23, PTC2, FMT0_CH1, BLUE */
	{ TEENSY_PIN23_IOPORT, TEENSY_PIN23, 1, 170 },
};

#define sduGet(sdup) chnGetTimeout((sdup), TIME_INFINITE)
#define sduPut(sdup, b) chnPutTimeout((sdup), (b), TIME_INFINITE)
#define sduWrite(sdup, str, len) chnWrite((sdup), (uint8_t *)(str), (len))

static int read_line(char *dst, size_t len)
{
	int escape = 0;
	int bracket = 0;
	size_t pos = 0;
	len--;
	while (pos < len) {
		msg_t c = sduGet(&SDU1);
		if (c == Q_TIMEOUT || c == Q_RESET) {
			return -1;
		} else if (c == '\n' || c == '\r') {
			break;
		} else if (escape) {
			escape = 0;
			if (c == '[') {
				bracket = 1;
			}
		} else if (bracket) {
			bracket = 0;
			if (c == 'A') {
				/* UP */
			} else if (c == 'B') {
				/* DOWN */
			} else if (c == 'C') {
				/* LEFT */
			} else if (c == 'D') {
				/* RIGHT */
			}
		} else if (c == 0x1B)  {
			escape = 1;
		} else if (0x20 <= c && c < 0x7F) {
			sduPut(&SDU1, c);
			dst[pos] = c;
			pos++;
		} else if (c == 0x7F && pos > 0) {
			sduWrite(&SDU1, "\b \b", 3);
			pos--;
		} else {
			/*
			 * backspace ignored if pos == 0 along with
			 * other non printable characters
			 */
		}
	}
	sduWrite(&SDU1, "\r\n", 2);
	if (len) {
		dst[pos] = 0;
	}

	return pos;
}

static int split_args(char **argv, size_t len, char *line)
{
	int argc = 0;
	len--;
	while (len) {
		char *token = strsep(&line, " \t\r\n\a");
		if (!token)
			break;

		if (!token[0])
			continue;

		argc++;
		*argv++ = token;
		len--;
	}
	*argv = NULL;

	return argc;
}

static char *optarg;
static int optind;
static int optopt;
static int optpos;

static int getopt(int argc, char * const argv[], const char *optstr)
{
	/* reset: the first argument is never an option */
	if (!optind) {
		optind = 1;
		optpos = 0;
	}

	/* exit if we exceeded the argument count */
	if (optind >= argc || !argv[optind])
		return -1;

	/* exit if the argument doesn't start with '-' */
	if (argv[optind][0] != '-')
		return -1;

	/* exit if the option has no optopt */
	if (!argv[optind][1])
		return -1;

	/* "--" is the end: increase optind and terminate */
	if (argv[optind][1] == '-' && !argv[optind][2]) {
		optind++;
		return -1;
	}

	/* skip the initial '-' character */
	if (!optpos)
		optpos++;

	/* assign the optopt value */
	optopt = argv[optind][optpos];

	/* if there is nothing after the option go to the next
	 * argument */
	optpos++;
	if (!argv[optind][optpos]) {
		optind++;
		optpos = 0;
	}

	/* find the current option in the optstr */
	const char *d;
	for (d = optstr; *d  && *d != optopt; d++)
		;

	/* no such option in the optstring */
	if (d[0] != optopt)
		return '?';

	if (d[1] != ':') {
		/* the option needs no argument */
	} else if (optind >= argc) {
		/* the option needs an argument but no argument is
		 * available */
		return '?';
	} else {
		/*
		 * arg = 0 because it's optional and set only if there
		 * are no spaces between the option and its argument
		 * (see next conditional).
		 */
		if (d[2] == ':')
			optarg = 0;

		/*
		 * set optarg only when:
		 *
		 * 1. the arg is not optional (:)
		 *
		 * 2. the arg is optional (::) and there is no space
		 *    (optpos==0) between the option and the argument
		 *
		 * Then increase go to the next argument.
		 */
		if (d[2] != ':' || optpos) {
			optarg = argv[optind] + optpos;
			optind++;
			optpos = 0;
		}
	}

	return optopt;
}

static int repl_exit = 0;

static int exit_cmd(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	usbDisconnectBus(serusbcfg.usbp);
	chThdSleepMilliseconds(1500);
	usbStart(serusbcfg.usbp, &usbcfg);
	usbConnectBus(serusbcfg.usbp);

	repl_exit = 1;

	return 0;
}

static int help_cmd(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	const char *str =
		"Available commands:\r\n"
		"  help      this message\r\n"
		"  exit      exit the shell\r\n"
		"  setled    set the PWM value of leds (-rgb)\r\n"
		"            Options:\r\n"
		"             -r set the value of the red led\r\n"
		"             -g set the value of the green led\r\n"
		"             -b set the value of the blue led\r\n"
		"             no option: set the value of every led\r\n"
		"\r\n";

	sduWrite(&SDU1, str, strlen(str));
	return 0;
}

static int setled_cmd(int argc, char *argv[])
{
	uint8_t mask = 0;
	int opt;
	optind = 0;
	while ((opt = getopt(argc, argv, "rgb")) != -1) {
		switch (opt) {
		case 'r': mask |= 1; break;
		case 'g': mask |= 2; break;
		case 'b': mask |= 4; break;
		default:
			sduWrite(&SDU1, "wrong option\r\n", 16);
			return -1;
		}
	}

	/* no option == RGB */
	if (!mask) mask = 7;

	char *end;
	uint16_t value = strtoul(argv[optind], &end, 10);
	if (argv[optind] == end) {
		sduWrite(&SDU1, "wrong value\r\n", 15);
		return -1;
	}

	if (mask & 1) FTM0->CHANNEL[6].CnV = value;
	if (mask & 2) FTM0->CHANNEL[0].CnV = value;
	if (mask & 4) FTM0->CHANNEL[1].CnV = value;
	FTM0->PWMLOAD = FTM_PWMLOAD_LDOK_MASK;
	return 0;
}

static struct cmd_handler
{
	const char *name;
	int (*handler)(int argc, char *argv[]);
} handlers[] = {
	{ "exit",	exit_cmd },
	{ "help",	help_cmd },
	{ "setled",	setled_cmd },
	{ NULL, NULL },
};

static int execute_cmd(int argc, char *argv[])
{
	struct cmd_handler *t;
	for (t = handlers; t->name; t++) {
		if (strcmp(t->name, argv[0]) == 0)
			break;
	}

	if (t->name)
		return t->handler(argc, argv);

	/* dump the arguments */
	for(; *argv; argv++) {
		sduWrite(&SDU1, *argv, strlen(*argv));
		sduWrite(&SDU1, "*\r\n", 3);
	}
	return -1;
}

/*
 * shell thread
 */
static THD_WORKING_AREA(waShell, 256);
static THD_FUNCTION(shell, arg)
{
	(void)arg;
	chRegSetThreadName("shell");

	static char line[256];
	static int argc;
	static char *argv[10];

	const char *prompt = "control:~$ ";
	size_t plen = strlen(prompt);

	repl_exit = 0;
	while (!repl_exit) {
		/* prompt */
		sduWrite(&SDU1, prompt, plen);

		/* read */
		if (read_line(line, sizeof(line)) == -1)
			break;

		/* eval */
		argc = split_args(argv, sizeof(argv) / sizeof(argv[0]), line);
		if (argc)
			execute_cmd(argc, argv);

		/* loop */
	}
}


/*
 * Application entry point.
 */
int main(void) {

	/*
	 * System initializations.
	 * - HAL initialization, this also initializes the configured device drivers
	 *   and performs the board-specific initializations.
	 * - Kernel initialization, the main() function becomes a thread and the
	 *   RTOS is active.
	 */
	halInit();
	chSysInit();

	/*
	 * Initialize the hardware PWM.
	 */
	SIM->SCGC6 |= SIM_SCGC6_FTM0;
	FTM0->SC = FTM_SC_CLKS(1) | FTM_SC_PS(2);
	FTM0->MODE = FTM_MODE_FTMEN_MASK|FTM_MODE_PWMSYNC_MASK;
	FTM0->SYNC =  FTM_SYNC_CNTMIN_MASK|FTM_SYNC_CNTMAX_MASK
	 	|FTM_SYNC_SWSYNC_MASK;
	FTM0->COMBINE =  FTM_COMBINE_SYNCEN3_MASK | FTM_COMBINE_SYNCEN2_MASK
		| FTM_COMBINE_SYNCEN1_MASK | FTM_COMBINE_SYNCEN0_MASK;
	FTM0->SYNCONF =  FTM_SYNCONF_SYNCMODE_MASK;
	FTM0->MOD = 4096 - 1;
	struct pwm_desc *d = pwm_channels;
	for (unsigned i = 0; i < 3; i++, d++) {
		palSetPadMode(d->port, d->pad, PAL_MODE_ALTERNATIVE_4);
		FTM0->CHANNEL[d->ch].CnSC = FTM_CnSC_MSB | FTM_CnSC_ELSB;
		FTM0->CHANNEL[d->ch].CnV = 0;
	}
	FTM0->CNTIN = 0;
	FTM0->CNT = 0;
	FTM0->PWMLOAD = FTM_PWMLOAD_LDOK_MASK;

	/*
	 * Activates serial 1 (UART0) using the driver default configuration.
	 */
	sdStart(&SD1, NULL);

	/*
	 * Initialize and Activate SDU1 (USB CDC ACM).
	 */

	sduObjectInit(&SDU1);
	sduStart(&SDU1, &serusbcfg);

	/*
	 * Activates the USB driver and then the USB bus pull-up on D+.
	 * Note, a delay is inserted in order to not have to disconnect the cable
	 * after a reset.
	 */
	usbDisconnectBus(serusbcfg.usbp);
	chThdSleepMilliseconds(1500);
	usbStart(serusbcfg.usbp, &usbcfg);
	usbConnectBus(serusbcfg.usbp);

	/*
	 * Creates the blinker thread.
	 */
	chThdCreateStatic(waThread1, sizeof(waThread1), NORMALPRIO, Thread1, NULL);

	/*
	test_execute((BaseSequentialStream *)&SD1);
	*/
	while (true) {
		chThdSleepMilliseconds(250);
		if (SDU1.config->usbp->state == USB_ACTIVE) {
			thread_t *shelltp = chThdCreateStatic(
				waShell, sizeof(waShell), NORMALPRIO+1,
				shell, NULL);
			chThdWait(shelltp);
		}
	}

	return 0;
}
