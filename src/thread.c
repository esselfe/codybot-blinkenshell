#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <pthread.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <openssl/ssl.h>
#include <magic.h>

#include "codybot.h"

void *ThreadRXFunc(void *argp);
void ThreadRXStart(void) {
	pthread_t thr;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thr, &attr, ThreadRXFunc, NULL);
	pthread_detach(thr);
	pthread_attr_destroy(&attr);
}

void *ThreadRXFunc(void *argp) {
	while (!endmainloop) {
		memset(buffer_rx, 0, 4096);
		SSL_read(pSSL, buffer_rx, 4095);
		if (strlen(buffer_rx) == 0) {
			endmainloop = 1;
			break;
		}
		buffer_rx[strlen(buffer_rx)-2] = '\0';
		if (buffer_rx[0] != 'P' && buffer_rx[1] != 'I' && buffer_rx[2] != 'N' &&
		  buffer_rx[3] != 'G' && buffer_rx[4] != ' ')
			Log(IN, buffer_rx);
		// respond to ping request from the server with a pong
		if (buffer_rx[0] == 'P' && buffer_rx[1] == 'I' && buffer_rx[2] == 'N' &&
			buffer_rx[3] == 'G' && buffer_rx[4] == ' ' && buffer_rx[5] == ':') {
			sprintf(buffer_cmd, "%s\n", buffer_rx);
			buffer_cmd[1] = 'O';
			SSL_write(pSSL, buffer_cmd, strlen(buffer_cmd));
			if (debug)
				Log(OUT, buffer_cmd);
			memset(buffer_cmd, 0, 4096);
			continue;
		}

		RawLineParse(&raw, buffer_rx);
		RawGetTarget(&raw);

		int CheckCTCPTime(void) {
			t0 = time(NULL);
			if (strcmp(raw.nick, ctcp_prev_nick) == 0 && ctcp_prev_time <= t0-5) {
				ctcp_prev_time = t0;
				return 0;
			}
			else {
				ctcp_prev_time = t0;
				sprintf(ctcp_prev_nick, "%s", raw.nick);
				return 1;
			}
		}

		// Respond to CTCP version requests
		if (strcmp(raw.text, "\001CLIENTINFO\x01") == 0) {
			if (CheckCTCPTime())
				continue;

			sprintf(buffer, "NOTICE %s :\001CLIENTINFO CLIENTINFO PING TIME VERSION\x01\n",
				raw.nick);
			if (use_ssl)
				SSL_write(pSSL, buffer, strlen(buffer));
			else
				write(socket_fd, buffer, strlen(buffer));
			Log(OUT, buffer);
			continue;
		}
		else if (strncmp(raw.text, "\x01PING ", 6) == 0) {
			if (CheckCTCPTime())
				continue;

			sprintf(buffer, "NOTICE %s :%s\n", raw.nick, raw.text);
			if (use_ssl)
				SSL_write(pSSL, buffer, strlen(buffer));
			else
				write(socket_fd, buffer, strlen(buffer));
			Log(OUT, buffer);
			continue;
		}
		else if (strcmp(raw.text, "\x01TIME\x01") == 0) {
			if (CheckCTCPTime())
				continue;

			t0 = time(NULL);
			sprintf(buffer, "NOTICE %s :\x01TIME %s\x01\n", raw.nick, ctime(&t0));
			if (use_ssl)
				SSL_write(pSSL, buffer, strlen(buffer));
			else
				write(socket_fd, buffer, strlen(buffer));
			Log(OUT, buffer);
			continue;
		}
		else if (strcmp(raw.text, "\x01VERSION\x01") == 0) {
			if (CheckCTCPTime())
				continue;

			sprintf(buffer, "NOTICE %s :\x01VERSION codybot %s\x01\n", raw.nick,
				codybot_version_string);
			if (use_ssl)
				SSL_write(pSSL, buffer, strlen(buffer));
			else
				write(socket_fd, buffer, strlen(buffer));
			Log(OUT, buffer);
			continue;
		}

		if (strcmp(raw.channel, nick) == 0) {
			sprintf(buffer, "privmsg %s :Cannot use private messages\n", raw.nick);
			SSL_write(pSSL, buffer, strlen(buffer));
			continue;
		}

if (raw.text != NULL && raw.nick != NULL && strcmp(raw.command, "JOIN") != 0 &&
strcmp(raw.command, "NICK")!=0) {
		SlapCheck(&raw);
// help
		if (raw.text[0]==trigger_char) {
		if (strncmp(raw.text+1, "help", 4) == 0) {
			char c = trigger_char;
sprintf(buffer, "commands: %cabout %cadmins %cascii %castro %ccal %ccalc "
"%cchars %ccolorize %cdate %chelp %cdict %cfoldoc %cfortune %cjoke %crainbow "
"%cstats %cuptime %cversion %cweather\n",
	c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c,c);
			Msg(buffer);
			continue;
		}
// about
		else if (strncmp(raw.text+1, "about", 5) == 0) {
			if (strcmp(nick, "codybot")==0)
				Msg("codybot is an IRC bot written in C by esselfe, "
					"sources @ https://github.com/esselfe/codybot-blinkenshell");
			else {
				sprintf(buffer, "codybot(%s) is an IRC bot written in C by esselfe, "
					"sources @ https://github.com/esselfe/codybot-blinkenshell", nick);
				Msg(buffer);
			}
		}
// admins
		else if (strcmp(raw.text+1, "admins reload") == 0) {
			DestroyAdminList();
			ParseAdminFile();
			char *str = EnumerateAdmins();
			sprintf(buffer, "Admins: %s\n", str);
			free(str);
			Msg(buffer);
		}
		else if (strncmp(raw.text+1, "admins", 6) == 0) {
			char *str = EnumerateAdmins();
			sprintf(buffer, "Admins: %s\n", str);
			free(str);
			Msg(buffer);
		}
// ascii
		else if (strncmp(raw.text+1, "ascii", 5) == 0) {
			if (strcmp(raw.channel, "#codybot")==0)
				AsciiArt(&raw);
			else
				Msg("ascii: can only be run in #codybot (due to output > 4 lines)");
		}
// astro
		else if (strcmp(raw.text+1, "astro") == 0) {
			sprintf(buffer, "astro: missing city argument, example: '%castro"
				" montreal'", trigger_char);
			Msg(buffer);
		}
		else if (strncmp(raw.text+1, "astro ", 6) == 0) {
			if (wttr_disabled) {
				sprintf(buffer, "!astro is currently disabled, try again later or ask an admin to enable it");
				Msg(buffer);
			}
			else
				Astro(&raw);
		}
// cal
		else if (strcmp(raw.text+1, "cal") == 0)
			Cal();
// calc
		else if (strcmp(raw.text+1, "calc") == 0)
			Msg("calc  example: '!calc 10+20'");
		else if (strncmp(raw.text+1, "calc ", 5) == 0)
			Calc(&raw);
// chars
		else if (strncmp(raw.text+1, "chars", 5) == 0)
			Chars(&raw);
// colorize
		else if (strcmp(raw.text+1, "colorlist") == 0) {
			Msg("\003011\003022\003044\003055\003066\003077\003088\00309\0031010\0031111\0031212\0031313\0031414\0031515");
		}
		else if (strcmp(raw.text+1, "colorize") == 0) {
			sprintf(buffer, "Usage: %ccolorize some text to process", trigger_char);
			Msg(buffer);
		}
		else if (strncmp(raw.text+1, "colorize ", 9) == 0)
			Colorize(&raw);
// date
		else if (strlen(raw.text) >= 6 &&
			strncmp(raw.text+1, "date utc", 8) == 0) {
			if (strncmp(raw.text+5, "utc-12", 6) == 0)
				Date(-12);
			else if (strncmp(raw.text+6, "utc-11", 6) == 0)
				Date(-11);
			else if (strncmp(raw.text+6, "utc-10", 6) == 0)
				Date(-10);
			else if (strncmp(raw.text+6, "utc-9", 5) == 0)
				Date(-9);
			else if (strncmp(raw.text+6, "utc-8", 5) == 0)
				Date(-8);
			else if (strncmp(raw.text+6, "utc-7", 5) == 0)
				Date(-7);
			else if (strncmp(raw.text+6, "utc-6", 5) == 0)
				Date(-6);
			else if (strncmp(raw.text+6, "utc-5", 5) == 0)
				Date(-5);
			else if (strncmp(raw.text+6, "utc-4", 5) == 0)
				Date(-4);
			else if (strncmp(raw.text+6, "utc-3", 5) == 0)
				Date(-3);
			else if (strncmp(raw.text+6, "utc-2", 5) == 0)
				Date(-2);
			else if (strncmp(raw.text+6, "utc-1", 5) == 0)
				Date(-1);
			else if (strncmp(raw.text+6, "utc+1", 5) == 0)
				Date(1);
			else if (strncmp(raw.text+6, "utc+2", 5) == 0)
				Date(2);
			else if (strncmp(raw.text+6, "utc+3", 5) == 0)
				Date(3);
			else if (strncmp(raw.text+6, "utc+4", 5) == 0)
				Date(4);
			else if (strncmp(raw.text+6, "utc+5", 5) == 0)
				Date(5);
			else if (strncmp(raw.text+6, "utc+6", 5) == 0)
				Date(6);
			else if (strncmp(raw.text+6, "utc+7", 5) == 0)
				Date(7);
			else if (strncmp(raw.text+6, "utc+8", 5) == 0)
				Date(8);
			else if (strncmp(raw.text+6, "utc+9", 5) == 0)
				Date(9);
			else if (strncmp(raw.text+6, "utc+10", 6) == 0)
				Date(10);
			else if (strncmp(raw.text+6, "utc+11", 6) == 0)
				Date(11);
			else if (strncmp(raw.text+6, "utc+12", 6) == 0)
				Date(12);
		}
		else if (strncmp(raw.text+1, "date", 4) == 0)
			Date(0);
// dict
		else if (strcmp(raw.text+1, "dict") == 0) {
			sprintf(buffer, "Missing term argument, e.g. '%cdict wordhere'", trigger_char);
			Msg(buffer);
		}
		else if (strncmp(raw.text+1, "dict ", 5) == 0)
			Dict(&raw);
// foldoc
		else if (strcmp(raw.text+1, "foldoc") == 0) {
			sprintf(buffer, "Missing term argument, e.g. '%cfoldoc linux' "
				" (foldoc is a computer-related dictionary)", trigger_char);
			Msg(buffer);
		}
		else if (strncmp(raw.text+1, "foldoc ", 7) == 0)
			Foldoc(&raw);
// fortune
		else if (strncmp(raw.text+1, "fortune", 7) == 0)
			Fortune(&raw);
		else if (strcmp(raw.text+1, "debug on") == 0) {
			if (IsAdmin(raw.nick, raw.host)) {
				debug = 1;
				Msg("debug = 1");
			}
			else
				Msg("debug mode can only be changed by an admin");
		}
		else if (strcmp(raw.text+1, "debug off") == 0) {
			if (IsAdmin(raw.nick, raw.host)) {
				debug = 0;
				Msg("debug = 0");
			}
			else
				Msg("debug mode can only be changed by an admin");
		}
		else if (strncmp(raw.text+1, "die", 4) == 0) {
			if (IsAdmin(raw.nick, raw.host))
				exit(0);
		}
		else if (strncmp(raw.text+1, "joke", 4) == 0)
			Joke(&raw);
		else if (strcmp(raw.text+1, "msgbig") == 0 &&
			IsAdmin(raw.nick, raw.host)) {
			memset(buffer, 0, 4096);
			memset(buffer, '#', 1024);
			Msg(buffer);
		}
// rainbow
		else if (strcmp(raw.text+1, "rainbow") == 0) {
			sprintf(buffer, "Usage: %crainbow some random text", trigger_char);
			Msg(buffer);
		}
		else if (strncmp(raw.text+1, "rainbow ", 8) == 0)
			Rainbow(&raw);
// stats
		else if (strcmp(raw.text+1, "stats") == 0)
			Stats(&raw);
// trigger
		else if (strcmp(raw.text+1, "trigger") == 0) {
			sprintf(buffer, "trigger = '%c'", trigger_char);
			Msg(buffer);
		}
		else if (raw.text[1]=='t'&&raw.text[2]=='r'&&
			raw.text[3]=='i'&&raw.text[4]=='g'&&raw.text[5]=='g'&&raw.text[6]=='e'&&
			raw.text[7]=='r'&&raw.text[8]==' '&&raw.text[9]!='\n') {
			if (IsAdmin(raw.nick, raw.host))
				trigger_char = raw.text[9];
			sprintf(buffer, "trigger = '%c'", trigger_char);
			Msg(buffer);
		}
		else if (strcmp(raw.text+1, "uptime") == 0)
			Uptime(&raw);
		else if (strcmp(raw.text+1, "version") == 0) {
			sprintf(buffer, "codybot %s", codybot_version_string);
			Msg(buffer);
		}
		else if (strcmp(raw.text+1, "weather") == 0) {
			sprintf(buffer, "weather: missing city argument, example: '%cweather montreal'", trigger_char);
			Msg(buffer);
		}
		else if (strncmp(raw.text+1, "weather ", 8) == 0) {
			if (wttr_disabled) {
				sprintf(buffer, ",weather is currently disabled, try again later or ask an admin to enable it");
				Msg(buffer);
			}
			else
				Weather(&raw);
		}
		else if (strcmp(raw.text+1, "weather_disable") == 0) {
			if (IsAdmin(raw.nick, raw.host)) {
				wttr_disabled = 1;
				Msg("weather_disabled = 1");
			}
			else {
				sprintf(buffer, "Only an admin can use ,weather_disable");
				Msg(buffer);
			}
		}
		else if (strcmp(raw.text+1, "weather_enable") == 0) {
			if (IsAdmin(raw.nick, raw.host)) {
				wttr_disabled = 0;
				Msg("weather_disabled = 0");
			}
			else {
				sprintf(buffer, "Only an admin can use ,weather_enable");
				Msg(buffer);
			}
		}
		} // if (raw.text[0]==trigger_char)

		usleep(10000);
}
	}
	return NULL;
}

