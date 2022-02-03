FLAGS= -Wall -g -pthread
DEPENDENCIES = server_helpers.h pipelined_helpers.h

all : reg_server persistent_server pipelined_server
 
reg_server : server.o server_helpers.o
	gcc ${FLAGS} -o $@ $^

persistent_server : PersistentServer.o server_helpers.o
	gcc ${FLAGS} -o $@ $^

pipelined_server : PipelinedServer.o pipelined_helpers.o
	gcc ${FLAGS} -o $@ $^

%.o : %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

clean:
	rm -f *.o _server
	rm -f reg_server pipelined_server persistent_server
	rm -f logfile.txt

