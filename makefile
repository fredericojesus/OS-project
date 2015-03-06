all: simulador monitor

monitor: monitor.o
	gcc -Wall -g monitor.o -o monitor -lpthread
monitor.o: monitor.c util.h unix.h
	gcc -c monitor.c
simulador: simulador.o
	gcc -Wall -g simulador.o -o simulador -lpthread
simulador.o: simulador.c util.h unix.h
	gcc -c simulador.c
clean:
	rm *.o
	rm simulador
	rm monitor
	rm relatorio.log
