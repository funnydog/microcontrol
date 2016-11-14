/*
  ChibiOS - Copyright (C) 2006..2015 Giovanni Di Sirio

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

  http://www.apache.org/licenses/LICENSE-2.0

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.
*/

#include "ch.h"
#include "hal.h"
#include "ch_test.h"

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

		time = serusbcfg.usbp->state == USB_ACTIVE ? 250 : 500;
		palClearPad(TEENSY_PIN13_IOPORT, TEENSY_PIN13);
		chThdSleepMilliseconds(time);
		palSetPad(TEENSY_PIN13_IOPORT, TEENSY_PIN13);
		chThdSleepMilliseconds(time);
	}
}

/* Breathing Sleep LED brighness(PWM On period) table
 *
 * http://www.wolframalpha.com/input/?i=%28sin%28+x%2F64*pi%29**8+*+255%2C+x%3D0+to+63
 * (0..63).each {|x| p ((sin(x/64.0*PI)**8)*255).to_i }
 */
/* ruby -e "a = ((0..255).map{|x| Math.exp(Math.cos(Math::PI+(2*x*(Math::PI)/255)))-Math.exp(-1) }); m = a.max; a.map\!{|x| (9900*x/m).to_i+1}; p a" */

#define BREATHE_STEP 16 /* ms; = 4000ms/TABLE_SIZE */
#define TABLE_SIZE 256
static const uint16_t breathing_table[TABLE_SIZE] = {
  1, 1, 2, 5, 8, 12, 17, 24, 31, 39, 48, 58, 69, 81, 95, 109, 124, 140, 158, 177, 196, 217, 239, 263, 287, 313, 340, 369, 399, 430, 463, 497, 532, 570, 608, 649, 691, 734, 779, 827, 875, 926, 979, 1033, 1090, 1148, 1209, 1271, 1336, 1403, 1472, 1543, 1617, 1693, 1771, 1852, 1935, 2021, 2109, 2199, 2292, 2387, 2486, 2586, 2689, 2795, 2903, 3014, 3127, 3243, 3362, 3482, 3606, 3731, 3859, 3989, 4122, 4256, 4392, 4531, 4671, 4813, 4957, 5102, 5248, 5396, 5545, 5694, 5845, 5995, 6147, 6298, 6449, 6600, 6751, 6901, 7050, 7198, 7344, 7489, 7632, 7772, 7911, 8046, 8179, 8309, 8435, 8557, 8676, 8790, 8900, 9005, 9105, 9201, 9291, 9375, 9454, 9527, 9593, 9654, 9708, 9756, 9797, 9831, 9859, 9880, 9894, 9900, 9901, 9894, 9880, 9859, 9831, 9797, 9756, 9708, 9654, 9593, 9527, 9454, 9375, 9291, 9201, 9105, 9005, 8900, 8790, 8676, 8557, 8435, 8309, 8179, 8046, 7911, 7772, 7632, 7489, 7344, 7198, 7050, 6901, 6751, 6600, 6449, 6298, 6147, 5995, 5845, 5694, 5545, 5396, 5248, 5102, 4957, 4813, 4671, 4531, 4392, 4256, 4122, 3989, 3859, 3731, 3606, 3482, 3362, 3243, 3127, 3014, 2903, 2795, 2689, 2586, 2486, 2387, 2292, 2199, 2109, 2021, 1935, 1852, 1771, 1693, 1617, 1543, 1472, 1403, 1336, 1271, 1209, 1148, 1090, 1033, 979, 926, 875, 827, 779, 734, 691, 649, 608, 570, 532, 497, 463, 430, 399, 369, 340, 313, 287, 263, 239, 217, 196, 177, 158, 140, 124, 109, 95, 81, 69, 58, 48, 39, 31, 24, 17, 12, 8, 5, 2, 1, 1
};

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

static THD_WORKING_AREA(waBreatheThread, 128);
static THD_FUNCTION(BreatheThread, arg)
{
	(void)arg;
	chRegSetThreadName("breather");
	while(true) {
/*
		uint32_t mask = 0;
		struct pwm_desc *d = pwm_channels;
		for (unsigned i = 0; i < 3; i++, d++) {
			FTM0->CHANNEL[d->ch].CnV =
				(uint32_t)breathing_table[d->pos] * 4096 / 10000;
			if (++d->pos >= TABLE_SIZE)
				d->pos = 0;
			mask |= 1 << d->ch;
		}
		FTM0->PWMLOAD = FTM_PWMLOAD_LDOK_MASK;
*/
		chThdSleepMilliseconds(BREATHE_STEP);
	}
}

#define sduGet(sdup) chnGetTimeout((sdup), TIME_INFINITE)
#define sduPut(sdup, b) chnPutTimeout((sdup), (b), TIME_INFINITE)

static size_t read_line(char *dst, size_t len)
{
	msg_t c;
	size_t pos = 0;
	len--;
	while (pos < len && (c = sduGet(&SDU1)) != '\n' && c != '\r') {
		if (c == Q_RESET) {
			break;
		} else if (c != 0x7F) {
			sduPut(&SDU1, c);
			dst[pos] = c;
			pos++;
		} else if (pos > 0) {
			sduPut(&SDU1, '\b');
			sduPut(&SDU1, ' ');
			sduPut(&SDU1, '\b');
			pos--;
		} else {
			/* backspace ignored if pos == 0 */
		}
	}
	sduPut(&SDU1, '\r');
	sduPut(&SDU1, '\n');
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
	char *d;
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

static int setled(int argc, char *argv[])
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
			chnWrite(&SDU1, "wrong option\r\n", 14);
			return -1;
		}
	}

	/* no option == RGB */
	if (!mask) mask = 7;

	char *end;
	uint16_t value = strtoul(argv[optind], &end, 10);
	if (argv[optind] == end) {
		chnWrite(&SDU1, "wrong value\r\n", 13);
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
	{ "setled", setled },
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
		chnWrite(&SDU1, *argv, strlen(*argv));
		chnWrite(&SDU1, "*\r\n", 3);
	}
	return -1;
}

static THD_WORKING_AREA(waShell, 256);
static THD_FUNCTION(shell, arg)
{
	(void)arg;
	chRegSetThreadName("shell");

	static char line[256];
	static int argc;
	static char *argv[10];

	while (true) {
		chnWrite(&SDU1, "$ ", 2);

		read_line(line, sizeof(line));
		argc = split_args(argv, sizeof(argv) / sizeof(argv[0]), line);
		if (argc)
			execute_cmd(argc, argv);
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

	/* Initialize the hardware PWM */
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
		FTM0->CHANNEL[d->ch].CnSC = FTM_CnSC_MSB | FTM_CnSC_ELSA;
		FTM0->CHANNEL[d->ch].CnV = 0;
	}
	FTM0->CNTIN = 0;
	FTM0->CNT = 0;
	FTM0->PWMLOAD = FTM_PWMLOAD_LDOK_MASK;

	chThdCreateStatic(waBreatheThread, sizeof(waBreatheThread), NORMALPRIO, BreatheThread, NULL);

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

//	test_execute((BaseSequentialStream *)&SD1);
	while (true) {
		chThdSleepMilliseconds(250);

		if (SDU1.config->usbp->state == USB_ACTIVE) {
			thread_t *shelltp = chThdCreateFromHeap(NULL, 256, "shell", NORMALPRIO+1,
								shell, NULL);
			chThdWait(shelltp);
		}
	}

	return 0;
}
