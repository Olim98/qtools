CC       = gcc
LIBS     = -lreadline
CFLAGS   = -O2 -g -Wno-unused-result

OBJS     = hdlc.o  qcio.o memio.o

.PHONY: all clean

all:    qcommand qrmem qrflash qdload mibibsplit qwflash qwdirect qefs qnvram qblinfo

clean: 
	rm *.o
	rm $(all)

#.c.o:
#	$(CC) -o $@ $(LIBS) $^ qcio.o

qcio.o: qcio.c
hdlc.o: hdlc.c
memio.o: memio.c
#	$(CC) -c qcio.c

qcommand: qcommand.o  $(OBJS)
	gcc $^ -o $@ $(LIBS)

qrmem: qrmem.o $(OBJS)
	gcc $^ -o $@ $(LIBS)

qrflash: qrflash.o $(OBJS)
	gcc $^ -o $@ $(LIBS)

qwflash: qwflash.o $(OBJS)
	gcc $^ -o $@ $(LIBS)

#qwimage: qwimage.o $(OBJS)
#	gcc $^ -o $@ $(LIBS)

qdload: qdload.o $(OBJS)
	gcc $^ -o $@ $(LIBS)

qwdirect: qwdirect.o $(OBJS)
	gcc $^ -o $@ $(LIBS)
	
qefs  : qefs.o $(OBJS)
	gcc $^ -o $@ $(LIBS)

qnvram  : qnvram.o $(OBJS)
	gcc $^ -o $@ $(LIBS)
	
mibibsplit: mibibsplit.o $(OBJS)
	gcc $^ -o $@ $(LIBS)

qblinfo:    qblinfo.o
	gcc $^ -o $@ 
