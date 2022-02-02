FLAGS= -Wall -g
OBJ = server.o server_helpers.o
DEPENDENCIES = server_helpers.h

all : servers

servers : ${OBJ}
	gcc ${FLAGS} -o $@ $^

%.o : %.c ${DEPENDENCIES}
	gcc ${FLAGS} -c $<

clean:
	rm -f *.o servers

