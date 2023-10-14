master: master.c config.c nodo utente libreria.h
	gcc -std=c89 -pedantic master.c config.c libreria.h -o master

nodo: nodo.c config.c libreria.h
	gcc -std=c89 -pedantic nodo.c config.c libreria.h -o nodo

utente: utente.c config.c libreria.h
	gcc -std=c89 -pedantic utente.c config.c libreria.h -o utente


run:
	reset
	./master
