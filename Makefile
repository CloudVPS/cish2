#
# 
#
include makeinclude

OBJS		= args.o \
		  cmdtree.o \
		  main.o \
		  readpass.o \
		  tcrt.o \
		  terminus.o \
		  tgetchar.o

all: cish

clean:
	rm -f *.o cish

cish: $(OBJS)
	$(CC) -o cish $(OBJS) $(LIBS)

.SUFFIXES: .c .o
.c.o:
	$(CC) $(CFLAGS) -I. -c $<
