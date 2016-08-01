PROG    = LPAQ1v2
SRCS    = LPAQ1v2.c StateMap.c Mixer.c HashTable.c MatchModel.c Encoder.c
DEPS    = $(SRCS:.c=.d)
OBJS    = $(SRCS:.c=.o)

CFLAGS  += -DNDEBUG -Wall -O2 -Os -fomit-frame-pointer -std=c99
LDFLAGS += -Wl,--as-needed
LDLIBS  += 

RM      = rm -f

.SUFFIXES: .d
.c.d:
	$(CC) -MM $(CFLAGS) $< > $@

all:    $(PROG)

dep:    $(DEPS)

LPAQ1v2:$(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) $(OBJS) $(LDLIBS) -o $@

-include $(DEPS)

clean:
	$(RM) $(OBJS) $(PROG) $(DEPS)
