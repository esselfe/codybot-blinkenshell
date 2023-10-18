#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <openssl/ssl.h>
#include <pthread.h>

#include "codybot.h"

char *colors[] = {
	"\003", // default/restore
	"\00301", //black
//	"\00302", // blue --> too dark/unreadable
	"\00303", // green
	"\00304", // red
	"\00305", // brown
	"\00306", // purple
	"\00307", // orange
	"\00308", // yellow
	"\00309", // light green
	"\00310", // cyan
	"\00311", // light cyan
	"\00312", // light blue
	"\00313", // pink
	"\00314", // grey
	"\00315"}; // light grey

void AsciiArt(void) {
	FILE *fp = fopen("data/ascii.txt", "r");
	if (fp == NULL) {
		Msg("codybot::AsciiArt() error: cannot open data/ascii.txt");
		return;
	}

	// count how many '%'
	int c = 0;
	int cprev;
	int cnt = 0;
	while (1) {
		cprev = c;
		c = fgetc(fp);
		if (c == -1)
			break;
		else if (cprev == '\n' && c == '%')
			++cnt;
	}

	fseek(fp, 0, SEEK_SET);
	
	gettimeofday(&tv0, NULL);
	srand((unsigned int)tv0.tv_usec);
	int choice = 1;
	if (cnt > 0)
		choice = rand() % cnt;

	// go to chosen item
	fseek(fp, 0, SEEK_SET);
	if (choice > 0) {
		c = 0, cnt = 0;
		while (1) {
			cprev = c;
			c = fgetc(fp);
			if (c == -1)
				break;

			if (cprev == '\n' && c == '%')
				++cnt;
		
			if (cnt == choice) {
				c = fgetc(fp); // skip newline
				break;
			}
		}
	}

	char line[400];
	memset(line, 0, 400);
	cnt = 0, c = 0;
	while (1) {
		cprev = c;
		c = fgetc(fp);
		if (c == -1)
			break;
		else if (c == '%' && cprev == '\n')
			break;
		else if (c == '\n' || cnt >= 397) { // 397 + '\r\n\0' = 400
			Msg(line);
			// throttled due to server notice of flooding
			sleep(2);
			memset(line, 0, 400);
			cnt = 0;
		}
		else
			line[cnt++] = c;
	}

	fclose(fp);
}

// array containing time at which !astro have been run
unsigned long astro_usage[10];

// pop the first item
void AstroDecayUsage(void) {
	int cnt;
	for (cnt = 0; cnt < 9; cnt++)
		astro_usage[cnt] = astro_usage[cnt+1];

	astro_usage[cnt] = 0;
}

// return true if permitted, false if quota reached
int AstroCheckUsage(void) {
	int cnt;
	for (cnt = 0; cnt < 10; cnt++) {
		// if there's available slot
		if (astro_usage[cnt] == 0) {
			astro_usage[cnt] = time(NULL);
			return 1;
		}
		// if usage is complete and first item dates from over 30 minutes
		else if (cnt == 9 && astro_usage[0] < (time(NULL) - (60*30))) {
			AstroDecayUsage();
			astro_usage[cnt] = time(NULL);
			return 1;
		}
	}

	return 0;
}

void *AstroFunc(void *astro_ptr) {
	struct raw_line *rawp = RawLineDup((struct raw_line *)astro_ptr);

	if (!AstroCheckUsage()) {
		Msg("Astro quota reached, maximum 10 times every 30 minutes.");
		return NULL;
	}

	unsigned int cnt = 0;
	unsigned int cnt_conv = 0;
	char city[128];
	char city_conv[128];
	const char *cp = rawp->text + strlen("!astro ");
	memset(city, 0, 128);
	memset(city_conv, 0, 128);
	while (1) {
		if (*cp == '\n' || *cp == '\0' || cp - rawp->text >= 128)
			break;
		else if (cnt == 0 && *cp == ' ') {
			++cp;
			continue;
		}
		else if (*cp == '"' || *cp == '$' || *cp == '/' || *cp == '\\') {
			++cp;
			continue;
		}
		else if (*cp == ' ') {
			city[cnt++] = ' ';
			city_conv[cnt_conv++] = '%';
			city_conv[cnt_conv++] = '2';
			city_conv[cnt_conv++] = '0';
			++cp;
			continue;
		}
		
		city[cnt] = *cp;
		city_conv[cnt_conv] = *cp;
		++cnt;
		++cnt_conv;
		++cp;
	}
	RawLineFree(rawp);

	APIFetchAstro(city_conv);

	return NULL;
}

void Astro(struct raw_line *astro_rawp) {
	pthread_t thr;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thr, &attr, AstroFunc, (void *)astro_rawp);
	pthread_detach(thr);
	pthread_attr_destroy(&attr);
}

// Show calendars
void Cal(void) {
	// Change terminal highlight for IRC coloring codes
	system("cal -3 | sed 's/\x5F\x8 / /;s/\x5F\x8\\([0-9]\\)/\00308\\1\003/g' > cmd.output");

	FILE *fp = fopen("cmd.output", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Cal() error: Cannot open cmd.output: %s\n",
			strerror(errno));
		Msg(buffer);
		return;
	}

	size_t size = 1024;
	ssize_t ret;
	char *line = malloc(size);
	while (1) {
		memset(line, 0, size);
		ret = getline(&line, &size, fp);
		if (ret <= 0)
			break;
		else
			Msg(line);
	}

	free(line);
	fclose(fp);
}

void Calc(struct raw_line *calc_rawp) {
	strcat(calc_rawp->text, "\n");

	FILE *fi = fopen("cmd.input", "w+");
	if (fi == NULL) {
		sprintf(buffer, "codybot::Calc() error: Cannot open cmd.input for writing");
		Msg(buffer);
		return;
	}
	fputs("scale=2; ", fi);
	fputs(calc_rawp->text+6, fi);
	fclose(fi);

	system("/bin/bash -c './bc -l &> cmd.output < cmd.input'");

	FILE *fp = fopen("cmd.output", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Calc() error: Cannot open cmd.output: %s\n", strerror(errno));
		Msg(buffer);
		return;
	}

	char line[400];
	char *str;
	unsigned int cnt = 0;
	while (1) {
		str = fgets(line, 399, fp);
		if (str == NULL)
			break;
		Msg(line);
		++cnt;
		if (cnt >= 10) {
			system("cat cmd.output | nc esselfe.ca 9999 > cmd.url");
			FILE *fu = fopen("cmd.url", "r");
			if (fu == NULL) {
				sprintf(buffer, "##codybot::Calc() error: Cannot open cmd.url: %s\n", strerror(errno));
				Msg(buffer);
				break;
			}
			fgets(line, 399, fu);
			fclose(fu);
			Msg(line);
			break;
		}
	}

	fclose(fp);
}

void Chars(void) {
	Msg("https://esselfe.ca/chars.html");

	FILE *fp = fopen("data/chars.txt", "r");
	if (fp == NULL) {
		Msg("codybot::Chars() error: Cannot open data/chars.txt: %s\n");
		return;
	}

	char chars_line[4096];
	char *str;
	while (1) {
		str = fgets(chars_line, 4095, fp);
		if (str == NULL) break;
		sprintf(buffer, "%s\n", chars_line);
		Msg(buffer);
	}

	fclose(fp);
}

void Colorize(struct raw_line *colorize_rawp) {
	struct raw_line *rawp = RawLineDup(colorize_rawp);
	const char *cp = rawp->text + strlen("!colorize ");
	
	char result[4096];
	memset(result, 0, 4096);
	while (1) {
		if (*cp == '\0')
			break;
		strcat(result, colors[(rand()%13)+2]);
		strncat(result, cp++, 1);
	}
	strcat(result, colors[0]);
	
	RawLineFree(rawp);

	Msg(result);
}

void Date(int offset) {
	if (offset == 0)
		sprintf(buffer, "TZ=UTC date > cmd.output");
	else if (offset < 0)
		sprintf(buffer, "TZ=UTC+%d date > cmd.output; sed -i 's/ UTC//' cmd.output", -offset);
	else if (offset > 0)
		sprintf(buffer, "TZ=UTC-%d date > cmd.output; sed -i 's/ UTC//' cmd.output", offset);

	system(buffer);

	FILE *fp = fopen("cmd.output", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Date() error: Cannot open cmd.output: %s\n",
			strerror(errno));
		Msg(buffer);
		return;
	}

	size_t size = 1024;
	char *line = malloc(size);
	memset(line, 0, 1024);
	getline(&line, &size, fp);
	Msg(line);
	free(line);

	fclose(fp);
}

// see https://tools.ietf.org/html/rfc2229 (not fully implemented)
void Dict(struct raw_line *dict_rawp) {
	struct raw_line *rawp = RawLineDup(dict_rawp);
	const char *cp = rawp->text + strlen("!dict ");
	char word[128];
	memset(word, 0, 128);
	int cnt;
	for (cnt=0; cnt<127; cp++,cnt++) {
		if (*cp == '\0')
			break;
		else if (*cp == ' ')
			break;

		word[cnt] = *cp;
	}
	
	RawLineFree(rawp);

	sprintf(buffer_cmd, "curl dict.org/d:%s:wn -o dict.output", word);
	Log(LOCAL, buffer_cmd);
	system(buffer_cmd);

	FILE *fp = fopen("dict.output", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Dict() error: Cannot open dict.output: %s\n",
			strerror(errno));
		Msg(buffer);
		return;
	}
	FILE *fw = fopen("cmd.output", "w");
	if (fw == NULL) {
		sprintf(buffer, "##codybot::Dict() error: Cannot open cmd.output: %s\n",
			strerror(errno));
		Msg(buffer);
		fclose(fp);
		return;
	}

	size_t size = 1024;
	ssize_t bytes_read;
	char *line = malloc(size);
	int line_cnt = 0;
	while (1) {
		memset(line, 0, 1024);
		bytes_read = getline(&line, &size, fp);
		if (bytes_read == -1)
			break;

		if (strncmp(line, "220 ", 4) == 0 || strncmp(line, "250 ", 4) == 0 ||
			strncmp(line, "150 ", 4) == 0 || strncmp(line, "151 ", 4) == 0 ||
			strncmp(line, "221 ", 4) == 0 ||
			((line[0] > 65 && line[0] < 122) && line[0] != '.') ||
			strcmp(line, ".\r\n") == 0)
			continue;
		else if (strncmp(line, "552 ", 4) == 0) {
			Msg("No match");
			break;
		}
		else
			++line_cnt;

		if (line_cnt <= 4)
			Msg(line+4);

		fputs(line+4, fw);
	}
	fclose(fp);
	fclose(fw);

	if (line_cnt >= 5) {
		system("cat cmd.output | nc hobby.esselfe.ca 9999 > cmd.url");
		fp = fopen("cmd.url", "r");
		if (fp == NULL) {
			sprintf(buffer, "##codybot::Dict() error: Cannot open cmd.url: %s\n",
				strerror(errno));
			Msg(buffer);
			free(line);
			return;
		}

		bytes_read = getline(&line, &size, fp);
		if (bytes_read > -1)
			Msg(line);

		fclose(fp);
	}
	
	free(line);
}

void Foldoc(struct raw_line *foldoc_rawp) {
	struct raw_line *rawp = RawLineDup(foldoc_rawp);
	const char *cp = rawp->text + strlen("!foldoc ");
	char word[128];
	memset(word, 0, 128);
	int cnt;
	for (cnt=0; cnt<127; cp++,cnt++) {
		if (*cp == '\0')
			break;
		else if (*cp == ' ') {
			word[cnt++] = '%';
			word[cnt++] = '2';
			word[cnt] = '0';
			continue;
		}

		word[cnt] = *cp;
	}
	
	RawLineFree(rawp);

	sprintf(buffer_cmd, "curl dict.org/d:%s:foldoc -o dict.output", word);
	Log(LOCAL, buffer_cmd);
	system(buffer_cmd);

	FILE *fp = fopen("dict.output", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Foldoc() error: Cannot open dict.output: %s\n",
			strerror(errno));
		Msg(buffer);
		return;
	}
	FILE *fw = fopen("cmd.output", "w");
	if (fw == NULL) {
		sprintf(buffer, "##codybot::Foldoc() error: Cannot open cmd.output: %s\n",
			strerror(errno));
		Msg(buffer);
		fclose(fp);
		return;
	}

	size_t size = 1024;
	ssize_t bytes_read;
	char *line = malloc(size);
	int line_cnt = 0;
	while (1) {
		memset(line, 0, 1024);
		bytes_read = getline(&line, &size, fp);
		if (bytes_read == -1)
			break;

		if (strncmp(line, "220 ", 4) == 0 || strncmp(line, "250 ", 4) == 0 ||
			strncmp(line, "150 ", 4) == 0 || strncmp(line, "151 ", 4) == 0 ||
			strncmp(line, "221 ", 4) == 0 ||
			((line[0] > 65 && line[0] < 122) && line[0] != '.') ||
			strcmp(line, ".\r\n") == 0)
			continue;
		else if (strncmp(line, "552 ", 4) == 0) {
			Msg("No match");
			break;
		}
		else {
			if (strlen(line+3) > 0) {
				++line_cnt;
				if (line_cnt <= 10)
					Msg(line+3);

				fputs(line+3, fw);
			}
		}

	}
	fclose(fp);
	fclose(fw);

	if (line_cnt >= 11) {
		system("cat cmd.output | nc hobby.esselfe.ca 9999 > cmd.url");
		fp = fopen("cmd.url", "r");
		if (fp == NULL) {
			sprintf(buffer, "##codybot::Foldoc() error: Cannot open cmd.url: %s\n",
				strerror(errno));
			Msg(buffer);
			free(line);
			return;
		}

		bytes_read = getline(&line, &size, fp);
		if (bytes_read > -1)
			Msg(line);

		fclose(fp);
	}
	
	free(line);
}

// array containing time at which !astro have been run
unsigned long forecast_usage[10];

// pop the first item
void ForecastDecayUsage(void) {
	int cnt;
	for (cnt = 0; cnt < 9; cnt++)
		forecast_usage[cnt] = forecast_usage[cnt+1];

	forecast_usage[cnt] = 0;
}

// return true if permitted, false if quota reached
int ForecastCheckUsage(void) {
	int cnt;
	for (cnt = 0; cnt < 10; cnt++) {
		// if there's available slot
		if (forecast_usage[cnt] == 0) {
			forecast_usage[cnt] = time(NULL);
			return 1;
		}
		// if usage is complete and first item dates from over 30 minutes
		else if (cnt == 9 && forecast_usage[0] < (time(NULL) - (60*30))) {
			ForecastDecayUsage();
			forecast_usage[cnt] = time(NULL);
			return 1;
		}
	}

	return 0;
}

void *ForecastFunc(void *ptr) {
	struct raw_line *rawp = RawLineDup((struct raw_line *)ptr);

	if (!ForecastCheckUsage()) {
		Msg("Forecast quota reached, maximum 10 times every 30 minutes.");
		return NULL;
	}

	unsigned int cnt = 0;
	unsigned int cnt_conv = 0;
	char city[128];
	char city_conv[128];
	const char *cp = rawp->text + strlen("!forecast ");
	memset(city, 0, 128);
	memset(city_conv, 0, 128);
	while (1) {
		if (*cp == '\n' || *cp == '\0' || cp - rawp->text >= 128)
			break;
		else if (cnt == 0 && *cp == ' ') {
			++cp;
			continue;
		}
		else if (*cp == '"' || *cp == '$' || *cp == '/' || *cp == '\\') {
			++cp;
			continue;
		}
		else if (*cp == ' ') {
			city[cnt++] = ' ';
			city_conv[cnt_conv++] = '%';
			city_conv[cnt_conv++] = '2';
			city_conv[cnt_conv++] = '0';
			++cp;
			continue;
		}
		
		city[cnt] = *cp;
		city_conv[cnt_conv] = *cp;
		++cnt;
		++cnt_conv;
		++cp;
	}
	RawLineFree(rawp);

	APIFetchForecast(city_conv);

	return NULL;
}

void Forecast(struct raw_line *forecast_rawp) {
	pthread_t thr;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thr, &attr, ForecastFunc, (void *)forecast_rawp);
	pthread_detach(thr);
	pthread_attr_destroy(&attr);
}

void Fortune(struct raw_line *fortune_rawp) {
	FILE *fp = fopen("data/fortunes.txt", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Fortune() error: Cannot open data/fortunes.txt: %s",
			strerror(errno));
		Msg(buffer);
		return;
	}

	fseek(fp, 0, SEEK_END);
	unsigned long filesize = (unsigned long)ftell(fp);
	fseek(fp, 0, SEEK_SET);

	gettimeofday(&tv0, NULL);
	srand((unsigned int)tv0.tv_usec);
	// 150 to be remeasured if 2 last items in the file are deleted or new
	fseek(fp, rand()%(filesize-150), SEEK_CUR);

	int c = 0;
	int cprev;
	int cnt = 0;
	char fortune_line[4096];
	memset(fortune_line, 0, 4096);
	while (1) { // go to next entry following '%'
		cprev = c;
		c = fgetc(fp);
		if (c == -1) {
			c = ' ';
			break;
		}
		if (cprev == '\n' && c == '%') {
			//skip the newline
			fgetc(fp);
			c = ' ';
			break;
		}
	}

	if (debug) {
		sprintf(buffer, "&&&&fortune pos: %ld&&&&", ftell(fp));
		Log(LOCAL, buffer);
	}

	while (1) { // record the entry
		cprev = c;
		c = fgetc(fp);
		if (c == -1)
			break;
		else if (c == '\t' && cprev == '\n')
			break;
		else if (c == '%' && cprev == '\n')
			break;
		else if (c == '\n' && cprev == '\n')
			fortune_line[cnt++] = ' ';
		else if (c == '\n' && cprev != '\n')
			fortune_line[cnt++] = ' ';
		else
			fortune_line[cnt++] = c;
	}

	fclose(fp);

	if (strlen(fortune_line) > 0) {
		sprintf(buffer, "fortune: %s", fortune_line);
		Msg(buffer);
	
		// Read current stat count
		fp = fopen("stats", "r");
		if (fp == NULL) {
			sprintf(buffer, "codybot::Fortune() error: Cannot read stats file: %s", strerror(errno));
			Msg(buffer);
			return;
		}
		fgets(buffer, 4095, fp);
		fortune_total = atoi(buffer);
		fclose(fp);

		// Add 1 to stats
		fp = fopen("stats", "w");
		if (fp == NULL) {
			sprintf(buffer, "codybot::Fortune() error: Cannot write stats file: %s", strerror(errno));
			Msg(buffer);
			return;
		}
		fprintf(fp, "%llu\n", ++fortune_total);
		fclose(fp);
	}

}

void Joke(struct raw_line *joke_rawp) {
	struct raw_line *rawp = RawLineDup(joke_rawp);
	
	FILE *fp = fopen("data/jokes.txt", "r");
	if (fp == NULL) {
		sprintf(buffer, "codybot::Joke() error: cannot open data/jokes.txt: %s", strerror(errno));
		Msg(buffer);
		return;
	}

	fseek(fp, 0, SEEK_END);
	unsigned long filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	gettimeofday(&tv0, NULL);
	srand((unsigned int)tv0.tv_usec);
	unsigned int rnd = rand()%(filesize-100);
	fseek(fp, rnd, SEEK_CUR);
	if (debug)
		printf("##filesize: %lu\n##rnd: %u\n", filesize, rnd);

	int c = 0;
	int cprev;
	int cnt = 0;
	while (1) {
		cprev = c;
		c = fgetc(fp);
		if (c == -1) {
			break;
		}
		if (cprev == '\n' && c == '%') {
			//skip the newline
			fgetc(fp);
			break;
		}
	}

	char joke_line[4096];
	memset(joke_line, 0, 4096);
	cnt = 0;
	c = ' ';
	while (1) {
		cprev = c;
		c = fgetc(fp);
		if (c == -1)
			break;
		else if (c == '\t' && cprev == '\n')
			break;
		else if (c == '%' && cprev == '\n')
			break;
		else if (c == '\n' && cprev == '\n')
			joke_line[cnt++] = ' ';
		else if (c == '\n' && cprev != '\n')
			joke_line[cnt++] = ' ';
		else
			joke_line[cnt++] = c;
	}
	
	if (strlen(joke_line) > 0) {
		sprintf(buffer, "joke: %s", joke_line);
		Msg(buffer);
	}
	else {
		Msg("codybot::Joke(): joke_line is empty!");
	}
	RawLineFree(rawp);

	fclose(fp);
}

void Rainbow(struct raw_line *rainbow_rawp) {
	struct raw_line *rawp = RawLineDup(rainbow_rawp);
	const char *cp = rawp->text + strlen("!rainbow ");
	
	char *colors2[8] = {
	"\003", // restore/default
	"\00305", // red
	"\00304", // orange
	"\00308", // yellow
	"\00310", // green
	"\00311", // light cyan
	"\00312", // purple
	"\00302"}; // light cyan
	unsigned int cnt = 1;
	char result[4096];
	memset(result, 0, 4096);
	while (1) {
		if (*cp == '\0')
			break;
		
		strcat(result, colors2[cnt]);
		strncat(result, cp++, 1);
		if (*cp != ' ')
			++cnt;
		if (cnt >= 8)
			cnt = 1;
	}
	strcat(result, colors[0]);
	
	RawLineFree(rawp);

	Msg(result);
}

char *slap_items[20] = {
"an USB cord", "a power cord", "a laptop", "a slice of ham", "a keyboard", "a laptop cord",
"a banana peel", "a dictionary", "an atlas book", "a biography book", "an encyclopedia",
"a rubber band", "a large trout", "a rabbit", "a lizard", "a dinosaur",
"a chair", "a mouse pad", "a C programming book", "a belt"
};
unsigned int slap_max = 10;
unsigned int slap_cnt;
unsigned int slap_hour;
void SlapCheck(struct raw_line *slap_rawp) {
	struct raw_line *rawp = RawLineDup(slap_rawp);
	const char *c = rawp->text;
	
	if ((*c==1 && *(c+1)=='A' && *(c+2)=='C' && *(c+3)=='T' && *(c+4)=='I' &&
	  *(c+5)=='O' && *(c+6)=='N' && *(c+7)==' ' &&
	  *(c+8)=='s' && *(c+9)=='l' && *(c+10)=='a' && *(c+11)=='p' && *(c+12)=='s' &&
	  *(c+13)==' ') && 
	  ((*(c+14)=='c' && *(c+15)=='o' && *(c+16)=='d' && *(c+17)=='y' &&
	  *(c+18)=='b' && *(c+19)=='o' && *(c+20)=='t' && *(c+21)==' ') ||
	  (*(c+14)=='S' && *(c+15)=='p' && *(c+16)=='r' && *(c+17)=='i' &&
	  *(c+18)=='n' && *(c+19)=='g' && *(c+20)=='S' && *(c+21)=='p' && *(c+22)=='r' &&
	  *(c+23)=='o' && *(c+24)=='c' && *(c+25)=='k' && *(c+26)=='e' && *(c+27)=='t' &&
	  *(c+28)==' '))) {
		gettimeofday(&tv0, NULL);

		time_t slap_time = (time_t)tv0.tv_sec;
		struct tm *tm1 = malloc(sizeof(struct tm));
		gmtime_r(&slap_time, tm1);
		if (slap_cnt == 0) {
			slap_hour = tm0->tm_hour;
		}
		else {
			if (slap_hour != tm0->tm_hour) {
				slap_hour = tm0->tm_hour;
				slap_cnt = 0;
			}
			else if (slap_hour == tm0->tm_hour && slap_cnt >= slap_max) {
				RawLineFree(rawp);
				return;
			}
		}

		srand((unsigned int)tv0.tv_usec);
		sprintf(buffer, "%cACTION slaps %s with %s%c", 1, rawp->nick, slap_items[rand()%20], 1);
		Msg(buffer);
	}
	RawLineFree(rawp);
}

void Stats(struct raw_line *stats_rawp) {
	FILE *fp = fopen("stats", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Stats() error: Cannot open stats file: %s", strerror(errno));
		Msg(buffer);
		return;
	}
	else {
		char str[1024];
		fgets(str, 1023, fp);
		fclose(fp);
		fortune_total = atoi(str);
	}
	sprintf(buffer, "Given fortunes: %llu", fortune_total);
	Msg(buffer);
}

void Uptime(struct raw_line *uptime_rawp) {
	gettimeofday(&tv0, NULL);
	t0 = (time_t)tv0.tv_sec - tv_start.tv_sec;
	tm0 = gmtime(&t0);
	char buf[1024];
	if (tm0->tm_year - 70)
		sprintf(buf,
		  "uptime: %d year%s %d month%s %02d day%s %02d:%02d:%02d",
		  tm0->tm_year-70, ((tm0->tm_year-70)>1)?"s":"",
		  tm0->tm_mon, (tm0->tm_mon>1)?"s":"",
		  tm0->tm_mday-1, (tm0->tm_mday>2)?"s":"",
		  tm0->tm_hour, tm0->tm_min, tm0->tm_sec);
	else if (tm0->tm_mon > 0)
		sprintf(buf,
		  "uptime: %d month%s %02d day%s %02d:%02d:%02d",
		  tm0->tm_mon, (tm0->tm_mon>1)?"s":"",
		  tm0->tm_mday-1, (tm0->tm_mday>2)?"s":"",
		  tm0->tm_hour, tm0->tm_min, tm0->tm_sec);
	else if (tm0->tm_mday > 1)
		sprintf(buf, "uptime: %02d day%s %02d:%02d:%02d",
		  tm0->tm_mday-1, (tm0->tm_mday>2)?"s":"",
		  tm0->tm_hour, tm0->tm_min, tm0->tm_sec);
	else
		sprintf(buf, "uptime: %02d:%02d:%02d",
		  tm0->tm_hour, tm0->tm_min, tm0->tm_sec);
	
	Msg(buf);
}

// array containing time at which !weather have been run
unsigned long weather_usage[10];

// pop the first item
void WeatherDecayUsage(void) {
	int cnt;
	for (cnt = 0; cnt < 9; cnt++)
		weather_usage[cnt] = weather_usage[cnt+1];

	weather_usage[cnt] = 0;
}

// return true if permitted, false if quota reached
int WeatherCheckUsage(void) {
	int cnt;
	for (cnt = 0; cnt < 10; cnt++) {
		// if there's available slot
		if (weather_usage[cnt] == 0) {
			weather_usage[cnt] = time(NULL);
			return 1;
		}
		// if usage is complete and first item dates from over 30 minutes
		else if (cnt == 9 && weather_usage[0] < (time(NULL) - (60*30))) {
			WeatherDecayUsage();
			weather_usage[cnt] = time(NULL);
			return 1;
		}
	}

	return 0;
}

void *WeatherFunc(void *ptr) {
	struct raw_line *rawp = RawLineDup((struct raw_line *)ptr);
	
	char buf[4096]; // Use our own buffer instead of the global one
	memset(buf,0, 4096);

	if (!WeatherCheckUsage()) {
		Msg("Weather quota reached, maximum 10 times every 30 minutes.");
		return NULL;
	}

	unsigned int cnt = 0;
	unsigned int cnt_conv = 0;
	char city[128];
	char city_conv[128];
	const char *cp = rawp->text + strlen("!weather ");
	memset(city, 0, 128);
	memset(city_conv, 0, 128);
	while (1) {
		if (*cp == '\n' || *cp == '\0' || cp - rawp->text >= 128)
			break;
		else if (cnt == 0 && *cp == ' ') {
			++cp;
			continue;
		}
		else if (*cp == '"' || *cp == '$' || *cp == '/' || *cp == '\\') {
			++cp;
			continue;
		}
		else if (*cp == ' ') {
			city[cnt++] = ' ';
			city_conv[cnt_conv++] = '%';
			city_conv[cnt_conv++] = '2';
			city_conv[cnt_conv++] = '0';
			++cp;
			continue;
		}
		
		city[cnt] = *cp;
		city_conv[cnt_conv] = *cp;
		++cnt;
		++cnt_conv;
		++cp;
	}
	RawLineFree(rawp);

	APIFetchWeather(city_conv);

	return NULL;
}

void Weather(struct raw_line *weather_rawp) {
	pthread_t thr;
	pthread_attr_t attr;
	pthread_attr_init(&attr);
	pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
	pthread_create(&thr, &attr, WeatherFunc, (void *)weather_rawp);
	pthread_detach(thr);
	pthread_attr_destroy(&attr);
}

