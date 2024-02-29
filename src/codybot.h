#ifndef CODYBOT_H
#define CODYBOT_H 1

#include <sys/time.h>
#include <openssl/ssl.h>

#define CC_COMPILER_GCC 1
#define CC_COMPILER_TCC 2

// Globals from codybot.c
extern const char *codybot_version_string;
extern int debug;
extern int socket_fd;
extern int ret;
extern int endmainloop;
extern int cc_disabled;
extern int sh_disabled;
extern int sh_locked;
extern int wttr_disabled;
extern int cmd_timeout;
extern int use_ssl;
extern int cc_compiler;
extern unsigned long long fortune_total;
extern struct timeval tv0;
extern struct timeval tv_start;
extern struct tm *tm0;
extern time_t t0;
extern time_t ctcp_prev_time;
extern char ctcp_prev_nick[128];
extern char *log_filename;
extern char *buffer;
extern char *buffer_rx;
extern char *buffer_cmd;
extern char *buffer_log;
extern char trigger_char;
extern char *current_channel;
extern char *nick;
extern char *full_user_name;
extern char *hostname;
extern char *target;

// Globals from server.c
extern unsigned int server_port;
extern unsigned int local_port;
extern char *server_hostname;
extern char *server_ip;
extern char *server_ip_blinkenshell;
extern SSL *pSSL;

// Globals from raw.c
// a raw line from the server should hold something like one of these:
// :esselfe!~steph@user/esselfe PRIVMSG #codybot :!stats
// :codybot!~user@user/esselfe PRIVMSG ##linux-offtopic :!fortune
// :NickServ!NickServ@services. NOTICE codybot :Invalid password for codybot.
// :PING :1697621846
// :codybot MODE codybot :+Zi
// :copper.libera.chat 001 codybot :Welcome to the Libera.Chat Internet Relay Chat Network codybot
struct raw_line {
	char *nick;
	char *username;
	char *host;
	char *command;
	char *channel;
	char *text;
};
extern struct raw_line raw;

// from admin.c
struct Admin {
	struct Admin *prev;
	struct Admin *next;
	char *nick;
	char *host;
};

struct AdminList {
	unsigned int total_admins;
	struct Admin *first_admin;
	struct Admin *last_admin;
};
extern struct AdminList admin_list;

void AddAdmin(const char *newnick, const char *host);
void DestroyAdminList(void);
char *EnumerateAdmins(void);
int IsAdmin(const char *newnick, const char *host);
void ParseAdminFile(void);

// from api.c
void APIFetchAstro(char *city);
void APIFetchForecast(char *city);
void APIFetchTime(char *city);
void APIFetchWeather(char *city);

// from codybot.c
#define LOCAL 0
#define IN 1
#define OUT 2
void Log(unsigned int direction, const char *text);
void Msg(const char *text);
void *ThreadRXFunc(void *argp);

// from commands.c
void AsciiArt(void);
void Astro(struct raw_line *astro_rawp);
void Cal(void);
void Calc(struct raw_line *calc_rawp);
void CC(struct raw_line *rawp);
void Chars(void);
void Colorize(struct raw_line *colorize_rawp);
void Date(int offset);
void Dict(struct raw_line *dict_rawp);
void Foldoc(struct raw_line *foldoc_rawp);
void Forecast(struct raw_line *forecast_rawp);
void Fortune(struct raw_line *fortune_rawp);
void Joke(struct raw_line *joke_rawp);
void Ping(struct raw_line *ping_rawp);
void Rainbow(struct raw_line *rainbow_rawp);
void SlapCheck(struct raw_line *slap_rawp);
void Stats(struct raw_line *stats_rawp);
void Time(struct raw_line *time_rawp);
void Uptime(struct raw_line *uptime_rawp);
void Weather(struct raw_line *weather_rawp);

// from ctcp.c
int CheckCTCPTime(void);
int CheckCTCP(struct raw_line *rawp);

// from raw.c
char *RawGetTarget(struct raw_line *rawp);
void RawLineClear(struct raw_line *rawp);
struct raw_line *RawLineDup(struct raw_line *rawp);
void RawLineFree(struct raw_line *rawp);
void RawLineParse(struct raw_line *rawp, char *line);

// from server.c
void ServerGetIP(char *hostname);
void ServerConnect(void);
void ServerClose(void);

// from thread.c
void ThreadRunStart(char *command);
void *ThreadRunFunc(void *argp);
void ThreadRXStart(void);
void *ThreadRXFunc(void *argp);

#endif /* CODYBOT_H */
