ARGS = $(filter-out $@,$(MAKECMDGOALS))

all: libmsocket.a initmsocket sender receiver

libmsocket.a: msocket.o
	ar rcs libmsocket.a msocket.o

msocket.o: msocket.c msocket.h
	gcc -c -I. -fPIC -o $@ $<

initmsocket: initmsocket.c libmsocket.a
	gcc -I. -L. -o $@ $< -L. -lmsocket

sender: sender.c libmsocket.a
	gcc -I. -L. -o $@ $< -L. -lmsocket

receiver: receiver.c libmsocket.a
	gcc -I. -L. -o $@ $< -L. -lmsocket

runinit: initmsocket
	./initmsocket

runuser1: sender
	./sender $(ARGS)

runuser2: receiver
	./receiver $(ARGS)

clean:
	rm -f *.o *.a initmsocket sender receiver msocket.tar.gz

zip: msocket.c msocket.h initmsocket.c sender.c receiver.c makefile documentation.txt sample_100kB.txt
	tar -cvf msocket.tar.gz msocket.c msocket.h initmsocket.c sender.c receiver.c makefile documentation.txt sample_100kB.txt