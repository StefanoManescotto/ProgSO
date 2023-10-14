/*
	*Master
*/

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>

#include "libreria.h"

void stampaLibroMastro(struct blocco *, int *);
int calcolaBilanciUtenti();
int calcolaBilanciNodi();
void findFriends(pid_t);
void killAllChild();
void printAllBudgets();
void printImportantBudgets();
void printLastInfo();
void fine(char *);

int *block_index, secondPassed = 0, s_id, m_id, sh_id, sh_id2, sh_id_users, sh_id_nodes, *bilanciUtenti, *bilanciNodi;
char arg2[10], arg3[10], arg4[10], arg5[10], arg6[10];
struct blocco *libro_mastro;
struct msg_friends msg_f;
struct sembuf sops;


struct users_shm *users_shared;
struct nodes_shm *nodes_shared;


void signalHandler(int sig){
	int i = 0, forkResult, n;
	struct msg_transazione msg_t;
	pid_t *sf;

	switch(sig){
		case SIGALRM:

			secondPassed++;

			printf("\n-------------------------------------------------------\n\n");
			printf("secondPassed: %d/%d\n", secondPassed, SO_SIM_SEC);
			printf("Utenti Attivi: %d\nNodi Attivi: %d\n", users_shared->nUsers, nodes_shared->nNodes);

			if(secondPassed >= SO_SIM_SEC){
				sops.sem_op = -1;
				sops.sem_num = 1;
				semop(s_id, &sops, 1);

				killAllChild();

				printAllBudgets();

				sops.sem_op = 1;
				sops.sem_num = 1;
				semop(s_id, &sops, 1);

				/*stampaLibroMastro(libro_mastro, block_index);*/

				fine("Tempo Finito");
			}else{
				printImportantBudgets();

				alarm(1);
			}

			break;

		case SIGTERM:	/* --- Libro Mastro pieno --- */
			alarm(-1);

			killAllChild();


			printAllBudgets();


			/*stampaLibroMastro(libro_mastro, block_index);*/
			
			fine("Libro Mastro Pieno");
			
			break;

		case SIGUSR1: /* --- transazione inviata da un nodo --- */
			alarm(-1);

			msgrcv(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), getpid(), 0);

			sops.sem_flg = 0;
			sops.sem_op = 1;
		   	sops.sem_num = 0;
			semop(s_id, &sops, 1);

			if(nodes_shared->nNodes < SO_MAX_NODE_NUM){
				
				forkResult = fork();

				if(forkResult == -1){
					printf("errore\n");
					exit(EXIT_FAILURE);
				}else if(forkResult == 0){
					execlp("./nodo", "nodo", arg2, arg3, arg4, arg5, arg6, NULL);
				}


				msg_f.mtype = forkResult;
				findFriends(forkResult);
				msgsnd(m_id, &msg_f, sizeof(pid_t)*(SO_NUM_FRIENDS), 0);


				msg_t.mtype = forkResult;
				msgsnd(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), 0);

				sf = malloc(sizeof(pid_t) * SO_NUM_FRIENDS);

				for(i = 0; i < SO_NUM_FRIENDS; i++){

					do{
						n = rand()%nodes_shared->nNodes;
					}while(alreadyFriend(sf, nodes_shared->nodes[n], i));

					msg_f.mtype = 1;
					msg_f.pid_f[0] = forkResult;

				   	kill(nodes_shared->nodes[n], SIGUSR1);
				   	msgsnd(m_id, &msg_f, sizeof(pid_t)*(SO_NUM_FRIENDS), 0);

				   	sf[i] = nodes_shared->nodes[n];
				}

				free(sf);

				nodes_shared->nodes[nodes_shared->nNodes] = forkResult;
				nodes_shared->nNodes++;

			}else{ /* Raggiunto limite numero nodi */
				msg_t.mtype = msg_t.tr_msg.sender;

				msgsnd(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), 0);
				kill(msg_t.tr_msg.sender, SIGUSR1);

				semctl(s_id, 3, SETVAL, 1);
			}


			sops.sem_flg = 0;
			sops.sem_op = -1;
		   	sops.sem_num = 0;
			semop(s_id, &sops, 1);

			alarm(1);
			break;

		case SIGUSR2: /* --- tutti i processi utente sono terminati --- */
			alarm(-1);

			sops.sem_op = -1;
			sops.sem_num = 1;
			semop(s_id, &sops, 1);


			killAllChild();

			printAllBudgets();

			sops.sem_op = 1;
			sops.sem_num = 1;
			semop(s_id, &sops, 1);

			/*stampaLibroMastro(libro_mastro, block_index);*/

			fine("Utenti Terminati");

			break;
	}
}

int main(){
	int i = 0, j = 0, status;
	pid_t forkResult;
	struct sigaction sa, sa2, sa3;
	sigset_t my_mask;

	if(definisci() == -1){
		printf("valori inseriti errati\n");
		exit(EXIT_FAILURE);
	}

	bilanciUtenti = malloc(sizeof(int) * SO_USER_NUM);
	bilanciNodi = malloc(sizeof(int) * SO_MAX_NODE_NUM);

	setbuf(stdout, NULL);


	if (signal(SIGALRM, signalHandler) == SIG_ERR) {
		printf("\nErrore della disposizione dell'handler\n");
		exit(EXIT_FAILURE);
	}

	/* Setting the handler for SIGTERM */
	sigemptyset(&my_mask);
	sigaddset(&my_mask, SIGUSR1);
	sigaddset(&my_mask, SIGUSR2);

	sa.sa_mask = my_mask;
	sa.sa_handler = signalHandler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGTERM, &sa, NULL);


	/* Setting the handler for SIGUSR1 */
	sigemptyset(&my_mask);
	sigaddset(&my_mask, SIGTERM);
	sigaddset(&my_mask, SIGUSR2);

	sa2.sa_mask = my_mask;
	sa2.sa_handler = signalHandler;
	sa2.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa2, NULL);
	

	/* Setting the handler for SIGUSR2 */
	sigemptyset(&my_mask);
	sigaddset(&my_mask, SIGUSR1);
	sigaddset(&my_mask, SIGTERM);

	sa3.sa_mask = my_mask;
	sa3.sa_handler = signalHandler;
	sa3.sa_flags = SA_RESTART | SA_NODEFER;
	sigaction(SIGUSR2, &sa3, NULL);

	/* Shared Memory --- Libro Mastro */
	sh_id = shmget(IPC_PRIVATE, sizeof(struct blocco) * SO_REGISTRY_SIZE, 0600);
	if(sh_id == -1) {
		fprintf(stderr, "%s: %d. Errore in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		exit(EXIT_FAILURE);
	}

	/* Block Index Shared Memory ---- indice dell'ultimo blocco usato nel libro mastro */
	sh_id2 = shmget(IPC_PRIVATE, sizeof(int), 0600);
	if(sh_id2 == -1) {
		fprintf(stderr, "%s: %d. Errore in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		semctl(sh_id, 0, IPC_RMID);
		exit(EXIT_FAILURE);
	}

	/* Shared Memory --- Utenti */
	sh_id_users = shmget(IPC_PRIVATE, sizeof(struct users_shm), 0600);
	if(sh_id_users == -1) {
		fprintf(stderr, "%s: %d. Errore in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		semctl(sh_id, 0, IPC_RMID);
	   	semctl(sh_id2, 0, IPC_RMID);
		exit(EXIT_FAILURE);
	}

	/* Shared Memory --- Nodi */
	sh_id_nodes = shmget(IPC_PRIVATE, sizeof(struct nodes_shm), 0600);
	if(sh_id_nodes == -1) {
		fprintf(stderr, "%s: %d. Errore in shmget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		semctl(sh_id, 0, IPC_RMID);
	   	semctl(sh_id2, 0, IPC_RMID);
	   	semctl(sh_id_users, 0, IPC_RMID);
		exit(EXIT_FAILURE);
	}

	/* Coda di Messaggi */
	m_id = msgget(KEY, IPC_CREAT | 0600);
	if(m_id == -1) {
		fprintf(stderr, "%s: %d. Errore in msgget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		semctl(sh_id, 0, IPC_RMID);
	   	semctl(sh_id2, 0, IPC_RMID);
	   	semctl(sh_id_nodes, 0, IPC_RMID);
	   	semctl(sh_id_users, 0, IPC_RMID);
		exit(EXIT_FAILURE);
	}

	/* Semafori */
	s_id = semget(IPC_PRIVATE, 4, 0600);
	if(s_id == -1) {
		fprintf(stderr, "%s: %d. Errore in semget #%03d: %s\n", __FILE__, __LINE__, errno, strerror(errno));
		semctl(sh_id, 0, IPC_RMID);
	   	semctl(sh_id2, 0, IPC_RMID);
	   	semctl(sh_id_nodes, 0, IPC_RMID);
	   	semctl(sh_id_users, 0, IPC_RMID);
	   	msgctl(m_id, IPC_RMID, NULL);
		exit(EXIT_FAILURE);
	}

	libro_mastro = shmat(sh_id, NULL, 0);
	block_index = shmat(sh_id2, NULL, 0);
	users_shared = shmat(sh_id_users, NULL, 0);
	nodes_shared = shmat(sh_id_nodes, NULL, 0);

	for(i = 0; i < SO_REGISTRY_SIZE; i++){
		libro_mastro[i].id = -1;
	}

	*block_index = 0;
	
	semctl(s_id, 0, SETVAL, 0); /* Semaforo Num 0: Blocca tutti i processi figli finchè non sono tutti stati creati */
	semctl(s_id, 1, SETVAL, 1); /* Semaforo Num 1: Fa in modo che un solo nodo possa scrivere all'interno del libro mastro */
	semctl(s_id, 2, SETVAL, 1); /* Semaforo Num 2: Fa in modo che un solo processo nodo possa inviare un segnale al processo master (padre). Questo semaforo viene sbloccato 
									alla creazione di un nuovo nodo dopo la ricezione del segnale SIGUSR1 da parte del processo master */
	semctl(s_id, 3, SETVAL, 1); /* Semaforo Num 3: Fa in modo che un solo utente possa aggiornare il numero di utenti ancora "attivi" */

	sprintf(arg2, "%d", sh_id);
	sprintf(arg3, "%d", sh_id2);
	sprintf(arg4, "%d", s_id);
	sprintf(arg5, "%d", sh_id_nodes);
	sprintf(arg6, "%d", sh_id_users);
	
	sops.sem_flg = 0;
	sops.sem_op = 1;
   	sops.sem_num = 0;
	semop(s_id, &sops, 1);

	nodes_shared->nNodes = SO_NODES_NUM;
	users_shared->nUsers = SO_USER_NUM;

	for(i = 0; i < SO_USER_NUM + SO_NODES_NUM; i++){

		switch(forkResult = fork()){
			case -1:
				printf("errore");
				exit(EXIT_FAILURE);
			case 0:
				if(i < SO_NODES_NUM){
				 	execlp("./nodo", "nodo", arg2, arg3, arg4, arg5, arg6, NULL);
				}else{
					execlp("./utente", "utente", arg2, arg4, arg5, arg6, NULL);
				}

				exit(EXIT_FAILURE);
			default:
				if(i < SO_NODES_NUM){
					nodes_shared->nodes[i] = forkResult;
				}else{
					users_shared->users[i - SO_NODES_NUM] = forkResult;
				}

			break;
		}
	}

	for(i = 0; i < SO_NODES_NUM; i++){
   		msg_f.mtype = nodes_shared->nodes[i];
		findFriends(nodes_shared->nodes[i]);
	   	msgsnd(m_id, &msg_f, sizeof(pid_t)*(SO_NUM_FRIENDS), 0);
   	}

   	sops.sem_num = 0;
	sops.sem_op = -1;
	semop(s_id, &sops, 1);	

	alarm(1);

   	while(wait(&status) != -1){}

   	printLastInfo();

   	semctl(sh_id, 0, IPC_RMID);
   	semctl(sh_id2, 0, IPC_RMID);
   	semctl(sh_id_nodes, 0, IPC_RMID);
   	semctl(sh_id_users, 0, IPC_RMID);
   	semctl(s_id, 0, IPC_RMID);
   	msgctl(m_id, IPC_RMID, NULL);

   	free(bilanciUtenti);
   	free(bilanciNodi);

   	exit(EXIT_SUCCESS);
}


void stampaLibroMastro(struct blocco *libro_mastro, int *b_index){
	int i, j;
	printf("\n");
	for(i = 0; i < (*block_index); i++){
		printf("%-15s%-10s%-10s%-10s%-10s\n", "Timestamp", "Sender", "Receiver", "Quantity", "Reward"); 
		for(j = 0; j < SO_BLOCK_SIZE; j++){
    		printf("%-15d%-10d%-10d%-10d%-10d\n", libro_mastro[i].t[j].timestamp, libro_mastro[i].t[j].sender, libro_mastro[i].t[j].receiver,
														libro_mastro[i].t[j].quantity, libro_mastro[i].t[j].reward);

		}
		printf("\n------------------------------------------------------------\n");
	}
}

int calcolaBilanciUtenti(){
	int i, j, k, max = -1;

	
	/* --- assegno ai processi il budget iniziale: ---*/
	for(i = 0; i < SO_USER_NUM; i++){ 
		bilanciUtenti[i] = SO_BUDGET_INIT;
	}

	for(i = 0; i <  *block_index; i++){
		
		for(j = 0; j <  SO_BLOCK_SIZE - 1; j++){
			k = 0;
			
			while(k < SO_USER_NUM && libro_mastro[i].t[j].sender != users_shared->users[k]){
				k++;
			}
			
			if(libro_mastro[i].t[j].sender == users_shared->users[k]){
				bilanciUtenti[k] -= (libro_mastro[i].t[j].quantity + libro_mastro[i].t[j].reward);
			}


			k = 0;

			while(k < SO_USER_NUM && libro_mastro[i].t[j].receiver != users_shared->users[k]){
				k++;
			}

			if(libro_mastro[i].t[j].receiver == users_shared->users[k]){
				bilanciUtenti[k] += libro_mastro[i].t[j].quantity;

				if(bilanciUtenti[k] > max){
					max = bilanciUtenti[k];
				}
			}
		}
	}

	return max;
}

int calcolaBilanciNodi(){
	int i, j = SO_BLOCK_SIZE - 1, k, max = -1;

	
	/* --- assegno ai processi il budget iniziale: ---*/
	for(i = 0; i < nodes_shared->nNodes; i++){
		bilanciNodi[i] = 0;
	}

	for(i = 0; i <  *block_index; i++){

		k = 0;

		while(k < nodes_shared->nNodes && libro_mastro[i].t[j].receiver != nodes_shared->nodes[k]){
			k++;
		}

		if(libro_mastro[i].t[j].receiver == nodes_shared->nodes[k]){
			bilanciNodi[k] += libro_mastro[i].t[j].quantity;

			if(bilanciNodi[k] > max){
				max = bilanciNodi[k];
			}
		}
	}

	return max;
}


void findFriends(pid_t my_pid){
	int i = 0, j = 0;

	srand(getpid());

	for(i; i < SO_NUM_FRIENDS; i++){
		do{
			j = rand()%nodes_shared->nNodes;
		}while(nodes_shared->nodes[j] == my_pid || alreadyFriend(msg_f.pid_f, nodes_shared->nodes[j], i));

		msg_f.pid_f[i] = nodes_shared->nodes[j];
	}
}

int alreadyFriend(pid_t pidArray[], pid_t f, int index){
	int i = 0;

	for(i; i <= index; i++){
		if(pidArray[i] == f){
			return 1;
		}
	}

	return 0;
}

void killAllChild(){
	int i, status, exist;
	printf("\n");

	for(i = 0; i < nodes_shared->nNodes; i++){
		kill(nodes_shared->nodes[i], SIGINT);
	}


	for(i = 0; i < SO_USER_NUM; i++){
		exist = kill(users_shared->users[i], 0); /* Controllo se il process a cui inviare il segnale esiste ancora */

		if(exist != -1){
			kill(users_shared->users[i], SIGINT);
		}
	}

	while(wait(&status) != -1){}
}

void printAllBudgets(){
	int i;

	calcolaBilanciUtenti();
	calcolaBilanciNodi();

	printf("\n----------------------------------------------------\nBilancio Nodi:\n");

	for(i = 0; i < nodes_shared->nNodes; i++){
		printf("Nodo %d | %d\n", nodes_shared->nodes[i], bilanciNodi[i]);
	}

	printf("\nBilancio Utenti:\n");

	for(i = 0; i < SO_USER_NUM; i++){
		printf("Utente %d | %d\n", users_shared->users[i], bilanciUtenti[i]);
	}
}

void printImportantBudgets(){
	int i, maxU, maxN, cont = 0, nNodes = nodes_shared->nNodes;

	maxN = calcolaBilanciNodi();
	maxU = calcolaBilanciUtenti();

	maxN = maxN * 2/3;
	maxU = maxU * 2/3;

	for(i = 0; i < nNodes; i++){
		if((SO_NODES_NUM <= 5 || bilanciNodi[i] < 10 || bilanciNodi[i] >= maxN || (i >= nNodes - (5 - cont))) && cont <= 5){
			printf("Nodo %d | %d\n", nodes_shared->nodes[i], bilanciNodi[i]);
			cont++;
		}else if(cont > 5){
			break;
		}
	}

	cont = 0;

	for(i = 0; i < SO_USER_NUM; i++){
		if((SO_USER_NUM <= 10 || bilanciUtenti[i] < 10 || bilanciUtenti[i] >= maxU || (i >= SO_USER_NUM - (10 - cont))) && cont <= 10){
			printf("Utente %d | %d\n", users_shared->users[i], bilanciUtenti[i]);
			cont++;
		}else if(cont > 10){
			break;
		}
	}
}

void printLastInfo(){
	printf("Processi utente terminati prematuramente: %d\n", (SO_USER_NUM - users_shared->nUsers));
	printf("Numero di Blocchi nel Libro Mastro: %d\n", *block_index);
}

void fine(char *s){
	printf("\n%s\n", s);

	printLastInfo();

	semctl(sh_id, 0, IPC_RMID);
	semctl(sh_id2, 0, IPC_RMID);
	semctl(sh_id_nodes, 0, IPC_RMID);
	semctl(sh_id_users, 0, IPC_RMID);
	semctl(s_id, 0, IPC_RMID);
	msgctl(m_id, IPC_RMID, NULL);

	free(bilanciUtenti);
	free(bilanciNodi);

	exit(EXIT_SUCCESS);
}
