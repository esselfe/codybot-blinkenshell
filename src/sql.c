#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sqlite3.h>

void InitFortuneDB(void) {
	sqlite3 *db;

	int ret = sqlite3_open("codybot.db", &db);
	if (ret != SQLITE_OK) {
		printf("codybot error: cannot open db: %s\n", sqlite3_errmsg(db));
		sqlite3_close(db);
		return;
	}

	char *errmsg = NULL;
	char *SQL = malloc(100000);
	memset(SQL, 0, 100000);
	sprintf(SQL, "DROP TABLE IF EXISTS fortune;"
		"CREATE TABLE fortune (id INT, count INT);");
	int cnt;
	char buffer[1024];
	for (cnt = 1; cnt <= 368; cnt++) {
		sprintf(buffer, "INSERT INTO fortune VALUES (%d, 0);", cnt);
		strcat(SQL, buffer);
	}
	
	ret = sqlite3_exec(db, SQL, 0, 0, &errmsg);
	if (ret != SQLITE_OK) {
		printf("sqlite error: %s\n", errmsg);
		sqlite3_free(errmsg);
	}

	sqlite3_close(db);
}

