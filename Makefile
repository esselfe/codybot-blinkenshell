
CFLAGS = -std=c11 -Wall -Werror -D_GNU_SOURCE -I/home/codybot/opt/include
LDFLAGS = -lpthread -lssl -lcrypto -lmagic -L/home/codybot/opt/lib -lcurl -ljson-c
OBJDIR = obj
OBJS = $(OBJDIR)/admin.o $(OBJDIR)/api.o $(OBJDIR)/commands.o \
$(OBJDIR)/codybot.o $(OBJDIR)/ctcp.o $(OBJDIR)/raw.o \
$(OBJDIR)/server.o $(OBJDIR)/thread.o
PROGNAME = codybot

.PHONY: tcc default all prepare clean

default: all

all: prepare $(OBJS) $(PROGNAME)

prepare:
	@[ -d $(OBJDIR) ] || mkdir $(OBJDIR)

$(OBJDIR)/admin.o: src/admin.c
	gcc -c $(CFLAGS) src/admin.c -o $(OBJDIR)/admin.o

$(OBJDIR)/api.o: src/api.c
	gcc -c $(CFLAGS) src/api.c -o $(OBJDIR)/api.o

$(OBJDIR)/commands.o: src/commands.c
	gcc -c $(CFLAGS) src/commands.c -o $(OBJDIR)/commands.o

$(OBJDIR)/codybot.o: src/codybot.c
	gcc -c $(CFLAGS) src/codybot.c -o $(OBJDIR)/codybot.o

$(OBJDIR)/ctcp.o: src/ctcp.c
	gcc -c $(CFLAGS) src/ctcp.c -o $(OBJDIR)/ctcp.o

$(OBJDIR)/raw.o: src/raw.c
	gcc -c $(CFLAGS) src/raw.c -o $(OBJDIR)/raw.o

$(OBJDIR)/server.o: src/server.c
	gcc -c $(CFLAGS) src/server.c -o $(OBJDIR)/server.o

$(OBJDIR)/thread.o: src/thread.c
	gcc -c $(CFLAGS) src/thread.c -o $(OBJDIR)/thread.o

$(PROGNAME): $(OBJS)
	gcc $(CFLAGS) $(OBJS) -o $(PROGNAME) $(LDFLAGS)

clean:
	@rm -rv $(OBJDIR) $(PROGNAME) $(PROGRUN) 2>/dev/null || true

tcc:
	tcc -lmagic -lssl -o $(PROGNAME) src/*.c

