FLAGS= -Wall -g -pthread
OBJ = PipelinedServer.o pipelined_helpers.o
DEPENDENCIES = pipelined_helpers.h

all : servers

servers : ${OBJ}
	gcc ${FLAGS} -o $@ $^

%.o : %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

clean:
	rm -f *.o servers
	rm -f logfile.txt

