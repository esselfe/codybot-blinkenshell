/* Codybot is an IRC bot written in the C programming language,
   providing users with fortune cookies, jokes, weather per city,
   ascii-art images, text colorization and fortune cookie counter.
*/
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <getopt.h>
#include <signal.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#include "codybot.h"

// b stands for Blinkenshell's version
const char *codybot_version_string = "0.4.5b";

static const struct option long_options[] = {
	{"help", no_argument, NULL, 'h'},
	{"version", no_argument, NULL, 'V'},
	{"debug", no_argument, NULL, 'd'},
	{"hostname", required_argument, NULL, 'H'},
	{"log", required_argument, NULL, 'l'},
	{"fullname", required_argument, NULL, 'N'},
	{"nick", required_argument, NULL, 'n'},
	{"localport", required_argument, NULL, 'P'},
	{"port", required_argument, NULL, 'p'},
	{"server", required_argument, NULL, 's'},
	{"trigger", required_argument, NULL, 't'},
	{NULL, 0, NULL, 0}
};
static const char *short_options = "hVdH:l:N:n:P:p:s:t:";

void HelpShow(void) {
printf("Usage: codybot { OPTION [ ARGUMENT ] }\n"
"Options:\n"
"\t-h/--help\n"
"\t-V/--version\n"
"\t-d/--debug\n"
"\t-H/--hostname HOST\n"
"\t-l/--log FILENAME\n"
"\t-N/--fullname NAME\n"
"\t-n/--nick NICK\n"
"\t-P/--localport PORTNUM\n"
"\t-p/--port PORTNUM\n"
"\t-s/--server ADDR\n"
"\t-t/--trigger CHAR\n");
}

int debug;
int socket_fd;
int ret;
int endmainloop;
int wttr_disabled;
int use_ssl;
unsigned long long fortune_total;
struct timeval tv0;
struct timeval tv_start;
struct tm *tm0;
time_t t0;
time_t ctcp_prev_time;
char ctcp_prev_nick[128];
char *log_filename;
char *buffer;
char *buffer_rx;
char *buffer_cmd;
char *buffer_log;
char trigger_char;
char trigger_char_default = '!';
char *current_channel;
char *nick; // nick used by the bot
char *full_user_name;
char *hostname;

void Log(unsigned int direction, const char *text) {
	FILE *fp = fopen(log_filename, "a+");
	if (fp == NULL) {
		fprintf(stderr, "##codybot::Log() error: Cannot open %s: %s\n",
			log_filename, strerror(errno));
		return;
	}

	gettimeofday(&tv0, NULL);
	t0 = (time_t)tv0.tv_sec;
	tm0 = malloc(sizeof(struct tm));
	localtime_r(&t0, tm0);
	char *str = strdup(text);
	// remove trailing newline
	if (str[strlen(str)-1] == '\n')
		str[strlen(str)-1] = '\0';

	char dirstr[3];
	if (direction == LOCAL)
		sprintf(dirstr, "==");
	else if (direction == IN)
		sprintf(dirstr, "<<");
	else if (direction == OUT)
		sprintf(dirstr, ">>");
	else
		sprintf(dirstr, "::");

	sprintf(buffer_log, "%02d%02d%02d-%02d:%02d:%02d.%03ld %s##%s##\n",
		tm0->tm_year+1900-2000, tm0->tm_mon+1,
		tm0->tm_mday, tm0->tm_hour, tm0->tm_min, tm0->tm_sec, tv0.tv_usec,
		dirstr, str);
	fputs(buffer_log, fp);

	sprintf(buffer_log, "\033[00;36m%02d%02d%02d-%02d:%02d:%02d.%03ld %s##\033[00m%s\033[00;36m##\033[00m\n", 
		tm0->tm_year+1900-2000, tm0->tm_mon+1,
		tm0->tm_mday, tm0->tm_hour, tm0->tm_min, tm0->tm_sec, tv0.tv_usec,
		dirstr, str);
	free(str);
	fputs(buffer_log, stdout);
	
	memset(buffer_log, 0, 4096);

	fclose(fp);
}

void Msg(const char *text) {
	unsigned int total_len = strlen(text);
	if (total_len <= 400) {
		sprintf(buffer_log, "PRIVMSG %s :%s\n", target, text);
		if (use_ssl)
			SSL_write(pSSL, buffer_log, strlen(buffer_log));
		else
			write(socket_fd, buffer_log, strlen(buffer_log));
		Log(OUT, buffer_log);
		memset(buffer_log, 0, 4096);
	}
	else if (total_len > 400) {
		char str[401];
		const char *cp = text;
		unsigned int cnt;
		unsigned int cnt2 = 0;
		memset(str, 0, 401);
		sprintf(str, "PRIVMSG %s :", target);
		cnt = strlen(str);
		while (1) {
			str[cnt] = *(cp+cnt2);
			++cnt;
			++cnt2;
			if (cnt2 >= total_len) {
				str[cnt] = '\n';
				str[cnt+1] = '\0';
				if (use_ssl)
					SSL_write(pSSL, str, strlen(str));
				else
					write(socket_fd, str, strlen(str));
				Log(OUT, str);
				memset(str, 0, 400);
				break;
			}
			else if (cnt >= 399) {
				str[cnt] = '\n';
				str[cnt+1] = '\0';
				if (use_ssl)
					SSL_write(pSSL, str, strlen(str));
				else
					write(socket_fd, str, strlen(str));
				Log(OUT, str);
				memset(str, 0, 400);
				sprintf(str, "PRIVMSG %s :", target);
				cnt = strlen(str);
			}
		}
		return;
	}
}

// Read and parse keyboard input from the console
// on which codybot has been started from.
void ReadCommandLoop(void) {
	char buffer_line[4096];
	while (!endmainloop) {
		memset(buffer_line, 0, 4096);
		fgets(buffer_line, 4095, stdin);
		char *cp;
		cp = buffer_line;
		if (buffer_line[0] == '\n')
			continue;
		else if (strcmp(buffer_line, "exit\n") == 0 || strcmp(buffer_line, "quit\n") == 0)
			endmainloop = 1;
		else if (strcmp(buffer_line, "curch\n") == 0)
			printf("curch = %s\n", current_channel);
		else if (strncmp(buffer_line, "curch ", 6) == 0) {
			sprintf(current_channel, "%s", buffer_line+6);
		}
		else if (strcmp(buffer_line, "debug\n") == 0)
			printf("debug = %d\n", debug);
		else if (strcmp(buffer_line, "debug on\n") == 0) {
			debug = 1;
			Msg("debug = 1");
		}
		else if (strcmp(buffer_line, "debug off\n") == 0) {
			debug = 0;
			Msg("debug = 0");
		}
		else if (strcmp(buffer_line, "fortune\n") == 0) {
			sprintf(raw.channel, "%s", current_channel);
			Fortune(&raw);
		}
		else if (strncmp(buffer_line, "msg ", 4) == 0) {
			sprintf(buffer, "%s", buffer_line+4);
			sprintf(raw.channel, "%s", current_channel);
			target = raw.channel;
			Msg(buffer);
		}
		else if (strncmp(buffer_line, "id ", 3) == 0) {
			char *cp;
			cp = buffer_line+3;
			char pass[1024];
			unsigned int cnt = 0;
			while (1) {
				pass[cnt] = *cp;

				++cnt;
				++cp;
				if (*cp == '\n' || *cp == '\0')
					break;
			}
			pass[cnt] = '\0';

			sprintf(buffer_cmd, "PRIVMSG NickServ :identify %s\n", pass);
			if (use_ssl)
				SSL_write(pSSL, buffer_cmd, strlen(buffer_cmd));
			else
				write(socket_fd, buffer_cmd, strlen(buffer_cmd));
			Log(OUT, "PRIVMSG NickServ :identify *********");
			memset(buffer_cmd, 0, 4096);
		}
		else if (strcmp(buffer_line, "j\n") == 0) {
			sprintf(buffer_cmd, "JOIN #codybot\n");
			if (use_ssl)
				SSL_write(pSSL, buffer_cmd, strlen(buffer_cmd));
			else
				write(socket_fd, buffer_cmd, strlen(buffer_cmd));
			Log(OUT, buffer_cmd);
			memset(buffer_cmd, 0, 4096);
		}
		else if (strcmp(buffer_line, "trigger\n") == 0)
			printf("trigger = %c\n", trigger_char);
		else if (strncmp(buffer_line, "trigger ", 8) == 0) {
			if (*(cp+8)!='\n')
				trigger_char = *(cp+8);
			sprintf(buffer, "trigger_char = %c", trigger_char);
			Msg(buffer);
		}
		else if (strcmp(buffer_line, "uptime\n") == 0) {
			sprintf(raw.channel, "%s", current_channel);
			target = raw.channel;
			Uptime(NULL);
		}
		else { // Send input directly to the server
			if (use_ssl)
				SSL_write(pSSL, buffer_line, strlen(buffer_line));
			else
				write(socket_fd, buffer_line, strlen(buffer_line));
			memset(buffer_line, 0, 4096);
		}
	}
}

void SignalFunc(int signum) {
	ServerClose();
}

int main(int argc, char **argv) {
	// initialize time for !uptime
	gettimeofday(&tv_start, NULL);

	ctcp_prev_time = tv_start.tv_sec - 5;
	sprintf(ctcp_prev_nick, "codybot");

	signal(SIGINT, SignalFunc);

	int c;
	while (1) {
		c = getopt_long(argc, argv, short_options, long_options, NULL);
		if (c == -1)
			break;
		switch (c) {
		case 'h':
			HelpShow();
			exit(0);
		case 'V':
			printf("codybot %s\n", codybot_version_string);
			exit(0);
		case 'd':
			debug = 1;
			break;
		case 'H':
			if (optarg != NULL) {
				hostname = (char *)malloc(strlen(optarg)+1);
				sprintf(hostname, "%s", optarg);
			}
			break;
		case 'l':
			if (optarg != NULL) {
				log_filename = (char *)malloc(strlen(optarg)+1);
				sprintf(log_filename, "%s", optarg);
			}
			break;
		case 'N':
			if (optarg != NULL) {
				full_user_name = (char *)malloc(strlen(optarg)+1);
				sprintf(full_user_name, "%s", optarg);
			}
			break;
		case 'n':
			if (optarg != NULL) {
				nick = (char *)malloc(strlen(optarg)+1);
				sprintf(nick, "%s", optarg);
			}
			break;
		case 'P':
			if (optarg != NULL)
				local_port = atoi(optarg);
			break;
		case 'p':
			if (optarg != NULL)
				server_port = atoi(optarg);
			if (server_port == 6697)
				use_ssl = 1;
			break;
		case 's':
			if (optarg != NULL) {
				server_hostname = strdup(optarg);
				ServerGetIP(optarg);
			}
			break;
		case 't':
			if (optarg != NULL)
				trigger_char = *optarg;
			break;
		default:
			fprintf(stderr, "##codybot::main() error: Unknown argument: %c/%d\n", (char)c, c);
			break;
		}
	}

	ParseAdminFile();

	if (!full_user_name) {
		char *name = malloc(128);
		memset(name, 0, 128);
		getlogin_r(name, 127);
		full_user_name = (char *)malloc(strlen(name)+1);
		sprintf(full_user_name, "%s", name);
		free(name);
	}
	if (!hostname) {
		hostname = (char *)malloc(1024);
		gethostname(hostname, 1023);
	}
	if (!log_filename) {
		log_filename = (char *)malloc(strlen("codybot.log")+1);
		sprintf(log_filename, "codybot.log");
	}
	if (!current_channel) {
		current_channel = malloc(1024);
		sprintf(current_channel, "#codybot");
	}
	if (!nick) {
		nick = (char *)malloc(strlen("codybot")+1);
		sprintf(nick, "codybot");
	}
	if (!server_port) {
		server_port = 6697;
		use_ssl = 1;
	}
	if (server_port == 6667)
		use_ssl = 0;
	if (!server_hostname) {
		//const char *tmpstr = "irc.blinkenshell.org";
		const char *tmpstr = "sao.blinkenshell.org";
		server_hostname = malloc(strlen(tmpstr)+1);
		snprintf(server_hostname, strlen(tmpstr), "%s", tmpstr);
	}
	if (!server_ip)
		ServerGetIP(server_hostname);
	if (!trigger_char)
		trigger_char = trigger_char_default;

	raw.nick = (char *)malloc(1024);
	raw.username = (char *)malloc(1024);
	raw.host = (char *)malloc(1024);
	raw.command = (char *)malloc(1024);
	raw.channel = (char *)malloc(1024);
	raw.text = (char *)malloc(4096);

	buffer_rx = (char *)malloc(4096);
	memset(buffer_rx, 0, 4096);
	buffer_cmd = (char *)malloc(4096);
	memset(buffer_cmd, 0, 4096);
	buffer_log = (char *)malloc(4096);
	memset(buffer_log, 0, 4096);
	buffer = (char *)malloc(4096);
	memset(buffer, 0, 4096);

	FILE *fp = fopen("stats", "r");
	if (fp == NULL) {
		fprintf(stderr, "##codybot::main() error: Cannot open stats file: %s\n",
			strerror(errno));
	}
	else {
		char str[1024];
		fgets(str, 1023, fp);
		fclose(fp);
		fortune_total = atoi(str);
	}
	if (debug)
		printf("##fortune_total: %llu\n", fortune_total);

	ServerConnect();
	// mainloop
	ThreadRXStart();
	ReadCommandLoop();
	ServerClose();

	return 0;
}

