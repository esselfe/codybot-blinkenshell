#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <time.h>
#include <openssl/ssl.h>

#include "codybot.h"

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

int CheckCTCP(struct raw_line *rawp) {
	if (CheckCTCPTime())
		return 1;
	
	struct raw_line *raw2 = RawLineDup(rawp);
	
	// Respond to CTCP version requests
	if (strcmp(raw2->text, "\001CLIENTINFO\x01") == 0) {
		sprintf(buffer, "NOTICE %s :\001CLIENTINFO CLIENTINFO PING TIME VERSION\x01\n",
			raw2->nick);
		if (use_ssl)
			SSL_write(pSSL, buffer, strlen(buffer));
		else
			write(socket_fd, buffer, strlen(buffer));
		Log(OUT, buffer);
		return 1;
	}
	else if (strncmp(raw2->text, "\x01PING ", 6) == 0) {
		sprintf(buffer, "NOTICE %s :%s\n", raw2->nick, raw2->text);
		if (use_ssl)
			SSL_write(pSSL, buffer, strlen(buffer));
		else
			write(socket_fd, buffer, strlen(buffer));
		Log(OUT, buffer);
		return 1;
	}
	else if (strncmp(raw2->text, "\x01TIME\x01", 6) == 0) {
		t0 = time(NULL);
		char buf[128];
		ctime_r(&t0, buf);
		sprintf(buffer, "NOTICE %s :\x01TIME %s\x01\n", raw2->nick, buf);
		if (use_ssl)
			SSL_write(pSSL, buffer, strlen(buffer));
		else
			write(socket_fd, buffer, strlen(buffer));
		Log(OUT, buffer);
		return 1;
	}
	else if (strcmp(raw2->text, "\x01VERSION\x01") == 0) {
		sprintf(buffer, "NOTICE %s :\x01VERSION codybot %s\x01\n",
			raw2->nick, codybot_version_string);
		if (use_ssl)
			SSL_write(pSSL, buffer, strlen(buffer));
		else
			write(socket_fd, buffer, strlen(buffer));
		Log(OUT, buffer);
		return 1;
	}
	
	return 0;
}
