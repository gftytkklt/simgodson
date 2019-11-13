LIBS = -lm
#FLAGS = -Wall -O2 -fomit-frame-pointer -msse -march=athlon
FLAGS = -Wall -O0 -g
CC = gcc

SRCS = main.c time.c area.c io.c leakage.c basic_circuit.c def.h areadef.h leakage.h basic_circuit.h io.h time.h cacti_interface.h

OBJS = main.o time.o area.o io.o leakage.o basic_circuit.o

#all: cacti

LEXT = a
AR = ar qcv
RANLIB = ranlib

all: libcacti.$(LEXT)
	@echo "my work is done here..."

libcacti.$(LEXT):	$(OBJS)
	$(RM) libcacti.$(LEXT)
	$(AR) $(AROPT)libcacti.$(LEXT) $(OBJS)
	$(RANLIB) libcacti.$(LEXT)

pythonlib : time.o area.o io.o leakage.o basic_circuit.o cacti_wrap.o
		gcc -shared $(FLAGS) area.o time.o leakage.o basic_circuit.o io.o cacti_wrap.o -L /usr/lib/python2.4/config -lpython2.4 -o _cacti.so

cacti : main.o time.o area.o io.o leakage.o basic_circuit.o
	  $(CC) $(FLAGS) $(OBJS) -o cacti $(LIBS)

main.o : main.c def.h areadef.h leakage.h basic_circuit.h
	  $(CC) $(FLAGS) -c main.c -o main.o

leakage.o : leakage.h leakage.c
	  $(CC) $(FLAGS) -c leakage.c -o leakage.o

time.o :  time.c def.h areadef.h leakage.h basic_circuit.h cacti_interface.h
	   $(CC) $(FLAGS) -c time.c -o time.o

area.o : area.c def.h areadef.h cacti_interface.h
	   $(CC) $(FLAGS) -c area.c -o area.o 

io.o : def.h io.c areadef.h cacti_interface.h
	  $(CC) $(FLAGS) -c io.c -o io.o

basic_circuit.o : basic_circuit.h basic_circuit.c
		   gcc $(FLAGS) -c basic_circuit.c -o basic_circuit.o 

cacti_wrap.o :  cacti_wrap.c
		$(CC) -c io.c area.c time.c \
		basic_circuit.c leakage.c cacti_wrap.c \
		-I /usr/include/python2.4 \
		-I /usr/lib/python2.4/config

cacti_wrap.c: cacti.i
			swig -python cacti.i

clean:
	  rm -rf *.o *.a cacti

