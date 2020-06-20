CC = clang
CFLAGS = -Wall -Werror -g -pthread -std=gnu99
LDFLAGS = -pthread
OBJ = liftsim.o 
EXEC = lift_sim_B

$(EXEC) : $(OBJ)
	$(CC) $(OBJ) -o $(EXEC) -g $(LDFLAGS)
	
liftsim.o : liftsim.c liftsim.h request.h
			$(CC) $(CFLAGS) -c liftsim.c 

fileio.o : fileio.c fileio.h
			$(CC) $(CFLAGS) -c fileio.c

clean :
		rm -f $(OBJ) $(EXEC)
