#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/time.h>
#include <openssl/ssl.h>

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

void AsciiArt(struct raw_line *rawp) {
	FILE *fp = fopen("data-ascii.txt", "r");
	if (fp == NULL) {
		Msg("codybot::AsciiArt() error: cannot open data-ascii.txt");
		return;
	}

	fseek(fp, 0, SEEK_END);
	unsigned long filesize = ftell(fp);
	fseek(fp, 0, SEEK_SET);
	gettimeofday(&tv0, NULL);
	srand((unsigned int)tv0.tv_usec);
	fseek(fp, rand()%(filesize-100), SEEK_CUR);

	int c = 0, cprev, cnt = 0;
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

	char line[1024];
	memset(line, 0, 1024);
	cnt = 0, c = ' ';
	while (1) {
        cprev = c;
        c = fgetc(fp);
        if (c == -1)
            break;
        else if (c == '%' && cprev == '\n')
            break;
		else if (c == '\n') {
			Msg(line);
			// throttled due to server notice of flooding
			sleep(2);
			memset(line, 0, 1024);
			cnt = 0;
		}
        else
			line[cnt++] = c;
    }

	fclose(fp);
}

void Calc(struct raw_line *rawp) {
	// check for "kill" found in ",calc `killall codybot`" which kills the bot
    char *c = rawp->text;
    while (1) {
        if (*c == '\0' || *c == '\n')
            break;
		if (*c == '\\') {
			Msg("No backslashes allowed, sorry");
			return;
		}
        if (strlen(c) >= 5 && strncmp(c, "kill", 4) == 0) {
            Msg("calc: contains a blocked term...\n");
            return;
        }
        ++c;
    }

	// remove '^calc' from the line
	rawp->text += 6;
	strcat(rawp->text, "\n");

	FILE *fi = fopen("cmd.input", "w+");
	if (fi == NULL) {
		sprintf(buffer, "codybot::calc() error: Cannot open cmd.input for writing");
		Msg(buffer);
		return;
	}
	fputs(rawp->text, fi);
	fclose(fi);

	rawp->text -= 6;

	system("bc -l &> cmd.output < cmd.input");

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
		if (cnt >= 4) {
			system("cat cmd.output | ./nc termbin.com 9999 > cmd.url");
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

void Chars(struct raw_line *rawp) {
	Msg("https://esselfe.ca/chars.html");

	FILE *fp = fopen("data-chars.txt", "r");
	if (fp == NULL) {
		Msg("codybot::Chars() error: Cannot open data-chars.txt: %s\n");
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

void Colorize(struct raw_line *rawp) {
	char *cp = raw.text;

	while (*cp != ' ')
		++cp;
	++cp;
	
	char result[4096];
	memset(result, 0, 4096);
	while (1) {
		strcat(result, colors[(rand()%13)+2]);
		strncat(result, cp++, 1);
		if (*cp == '\0')
			break;
	}
	strcat(result, colors[0]);

	Msg(result);
}

// see https://tools.ietf.org/html/rfc2229 (not fully implemented)
void Dict(struct raw_line *rawp) {
	char *cp = rawp->text + strlen("!dict ");
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

	sprintf(buffer_cmd, "curl dict.org/d:%s:wn -o dict.output", word);
	Log(buffer_cmd);
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
		system("cat cmd.output | ./nc termbin.com 9999 > cmd.url");
		fp = fopen("cmd.url", "r");
		if (fp == NULL) {
			sprintf(buffer, "##codybot::Dict() error: Cannot open cmd.url: %s\n",
				strerror(errno));
			Msg(buffer);
			return;
		}

		bytes_read = getline(&line, &size, fp);
		if (bytes_read > -1)
			Msg(line);

		fclose(fp);
	}
}

void Foldoc(struct raw_line *rawp) {
	char *cp = rawp->text + strlen("!foldoc ");
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

	sprintf(buffer_cmd, "curl dict.org/d:%s:foldoc -o dict.output", word);
	Log(buffer_cmd);
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
		system("cat cmd.output | ./nc termbin.com 9999 > cmd.url");
		fp = fopen("cmd.url", "r");
		if (fp == NULL) {
			sprintf(buffer, "##codybot::Foldoc() error: Cannot open cmd.url: %s\n",
				strerror(errno));
			Msg(buffer);
			return;
		}

		bytes_read = getline(&line, &size, fp);
		if (bytes_read > -1)
			Msg(line);

		fclose(fp);
	}
}

void Fortune(struct raw_line *rawp) {
	FILE *fp = fopen("data-fortunes.txt", "r");
	if (fp == NULL) {
		sprintf(buffer, "##codybot::Fortune() error: Cannot open data-fortunes.txt: %s", strerror(errno));
		Msg(buffer);
		return;
	}

	fseek(fp, 0, SEEK_END);
	unsigned long filesize = (unsigned long)ftell(fp);
	fseek(fp, 0, SEEK_SET);
	gettimeofday(&tv0, NULL);
	srand((unsigned int)tv0.tv_usec);
	fseek(fp, rand()%(filesize-500), SEEK_CUR);

	int c = 0, cprev, cnt = 0;
	char fortune_line[4096];
	memset(fortune_line, 0, 4096);
	while (1) {
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
		Log(buffer);
	}

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
			fortune_line[cnt++] = ' ';
		else if (c == '\n' && cprev != '\n')
			fortune_line[cnt++] = ' ';
		else
			fortune_line[cnt++] = c;
	}

	fclose(fp);

	if (strlen(fortune_line) > 0) {
		RawGetTarget(rawp);
		sprintf(buffer, "fortune: %s", fortune_line);
		Msg(buffer);

		fp = fopen("stats", "r");
		if (fp == NULL) {
			sprintf(buffer, "codybot::Fortune() error: Cannot open stats file: %s", strerror(errno));
			Msg(buffer);
			return;
		}
		fgets(buffer, 4095, fp);
		fortune_total = atoi(buffer);
		fclose(fp);
	
		fp = fopen("stats", "w");
		if (fp == NULL) {
			sprintf(buffer, "codybot::Fortune() error: Cannot open stats file: %s", strerror(errno));
			Msg(buffer);
			return;
		}

		fprintf(fp, "%llu\n", ++fortune_total);

		fclose(fp);
	}

}

void Joke(struct raw_line *rawp) {
	FILE *fp = fopen("data-jokes.txt", "r");
	if (fp == NULL) {
		sprintf(buffer, "codybot::Joke() error: cannot open data-jokes.txt: %s", strerror(errno));
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

	int c = 0, cprev, cnt = 0;
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
	cnt = 0, c = ' ';
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

	RawGetTarget(rawp);
	if (strlen(joke_line) > 0) {
        sprintf(buffer, "joke: %s", joke_line);
		Msg(buffer);
    }
	else {
		Msg("codybot::Joke(): joke_line is empty!");
	}

	fclose(fp);
}

void Rainbow(struct raw_line *rawp) {
	char *cp = raw.text;

	while (*cp != ' ')
		++cp;
	++cp;
	
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
		strcat(result, colors2[cnt]);
		strncat(result, cp++, 1);
		if (*cp != ' ')
			++cnt;
		if (cnt >= 8)
			cnt = 1;
		if (*cp == '\0') {
			strcat(result, colors[0]);
			break;
		}
	}

	Msg(result);
}

char *slap_items[20] = {
"an USB cord", "a power cord", "a laptop", "a slice of ham", "a keyboard", "a laptop cord",
"a banana peel", "a dictionary", "an atlas book", "a biography book", "an encyclopedia",
"a rubber band", "a large trout", "a rabbit", "a lizard", "a dinosaur",
"a chair", "a mouse pad", "a C programming book", "a belt"
};
void SlapCheck(struct raw_line *rawp) {
	char *c = rawp->text;
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
		RawGetTarget(rawp);
		gettimeofday(&tv0, NULL);
		srand((unsigned int)tv0.tv_usec);
		sprintf(buffer, "%cACTION slaps %s with %s%c", 1, rawp->nick, slap_items[rand()%20], 1);
		Msg(buffer);
	}
}

void Stats(struct raw_line *rawp) {
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
	RawGetTarget(rawp);
	sprintf(buffer, "Given fortunes: %llu", fortune_total);
	Msg(buffer);
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

void Weather(struct raw_line *rawp) {
    if (!WeatherCheckUsage()) {
        Msg("Weather quota reached, maximum 10 times every 30 minutes.");
        return;
    }

	// check for "kill" found in ",weather `pkill${IFS}codybot`" which kills the bot
    char *c = rawp->text;
    while (1) {
        if (*c == '\0' || *c == '\n')
            break;
        if (strlen(c) >= 5 && strncmp(c, "kill", 4) == 0) {
            Msg("weather: contains a blocked term...\n");
            return;
        }
        ++c;
    }


	unsigned int cnt = 0;
	char city[1024], *cp = rawp->text + strlen("^weather ");
	memset(city, 0, 1024);
	while (1) {
		if (*cp == '\n' || *cp == '\0')
			break;
		else if (cnt == 0 && *cp == ' ') {
			++cp;
			continue;
		}
		else if (*cp == '"') {
			++cp;
			continue;
		}
		//else if (*cp == ' ')
		//	break;
		
		city[cnt] = *cp;
		++cnt;
		++cp;
	}
	memset(rawp->text, 0, strlen(rawp->text));

	char filename[1024];
	sprintf(filename, "/tmp/codybot-weather-%s", city);
	sprintf(buffer, "wget -t 1 -T 24 https://wttr.in/%s?format=%%C:%%t:%%f:%%w:%%p -O %s", city, filename);
	system(buffer);

	/*temp2[strlen(temp2)-1] = ' ';
	int deg_celsius = atoi(temp2);
	int deg_farenheit = (deg_celsius * 9 / 5) + 32;
	sprintf(buffer_cmd, "%s: %s %dC/%dF", city, temp, deg_celsius, deg_farenheit);
	Msg(buffer_cmd);*/

	FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        sprintf(buffer, "codybot error: Cannot open %s: %s", filename, strerror(errno));
        Msg(buffer);
        return;
    }
    fseek(fp, 0, SEEK_END);
    unsigned long filesize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    char *str = malloc(filesize+1);
    char *str2 = malloc(filesize+128);
	memset(str2, 0, filesize+128);
    fgets(str, filesize, fp);
    cnt = 0;
    int cnt2 = 0, step = 0;
    while (1) {
        if (str[cnt] == '\0') {
			str2[cnt2] = 'm';
            str2[cnt2+1] = '\n';
            str2[cnt2+2] = '\0';
            break;
        }
		else if (str[cnt] == ':') {
			str2[cnt2] = ' ';
			++cnt;
			++cnt2;
			++step;
			if (step == 2) {
				strcat(str2, "feels like ");
				cnt2 += 11;
			}
			continue;
		}
        else if (str[cnt] == -62 && str[cnt+1] == -80) {
            str2[cnt2] = '*';
            cnt += 2;
            ++cnt2;
            continue;
        }
		else if (str[cnt] < 32 || str[cnt] > 126) {
			++cnt;
			continue;
		}

        str2[cnt2] = str[cnt];
        ++cnt;
        ++cnt2;
    }
    sprintf(buffer, "%s: %s", city, str2);
    Msg(buffer);
	free(str);
	free(str2);
	
	if (!debug) {
		sprintf(buffer, "rm %s", filename);
		system(buffer);
		memset(buffer, 0, 4096);
	}
}
