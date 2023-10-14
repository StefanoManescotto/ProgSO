/*
	*Utente
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
#include <sys/msg.h>

#include "libreria.h"

#define LASTSENTSIZE 50 /* Dimensione dell'array contenente le ultime transazioni inviate */

void inviaTransazione();

int bilancio, lsIndex = 0, m_id, bilancioMastro, lastIndexI = 0;
struct blocco *libro_mastro;
struct transazione lastSent[LASTSENTSIZE];
struct msg_transazione msg_t;
struct timespec start, stop;


struct msqid_ds buf;


struct users_shm *users_shared;
struct nodes_shm *nodes_shared;

void signalHandler(int sig){

	struct msg_transazione full_msg;
	int i, num_messages;

	switch(sig){
		case SIGUSR1:
			msgrcv(m_id, &full_msg, sizeof(struct transazione) + (sizeof(int) * 2), getpid(), 0);
			for(i = 0; i < LASTSENTSIZE; i++){
				if(lastSent[i].timestamp == full_msg.tr_msg.timestamp && lastSent[i].receiver == full_msg.tr_msg.receiver){
					lastSent[i].timestamp = -1;
					break;
				}
			}

			break;
		case SIGUSR2:
			bilancio = calcolaBilancio(libro_mastro);

		  	msgctl(m_id, IPC_STAT, &buf);
		  	num_messages = buf.msg_qnum;

			if(bilancio >= 2 && num_messages < 100){
				inviaTransazione();
			}else{
				printf("Bilancio insufficente per inviare la transazione\n");
			}
		case SIGINT:
			exit(EXIT_SUCCESS);
	}
}

int main(int argc, char* argv[]){
	int i, j, receiverIndex, q, s_id = atoi(argv[2]), current_retry = 0;
	int num_messages;

	struct transazione nuovaTransazione;
	struct timespec tim;
	struct sembuf sops;

	struct sigaction sa;
	sigset_t my_mask;

	if(definisci() == -1){
		printf("valori inseriti errati\n");
		exit(EXIT_FAILURE);
	}

	bilancioMastro = SO_BUDGET_INIT;

	/* Setting the handler for SIGUSR1 */
	sigemptyset(&my_mask);
	sa.sa_mask = my_mask;
	sa.sa_handler = signalHandler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);

	/* Setting the handler for SIGUSR2 */
	sa.sa_mask = my_mask;
	sa.sa_handler = signalHandler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR2, &sa, NULL);


	/* Setting the handler for SIGINT */
	sa.sa_mask = my_mask;
	sa.sa_handler = signalHandler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGINT, &sa, NULL);


	srand(getpid());

	m_id = msgget(KEY, IPC_CREAT | 0600);

	libro_mastro = shmat(atoi(argv[1]), NULL, 0);

	nodes_shared = shmat(atoi(argv[3]), NULL, 0);
	users_shared = shmat(atoi(argv[4]), NULL, 0);

	msg_t.flag = 0;

	for(i = 0; i < LASTSENTSIZE; i++){
		lastSent[i].timestamp = -1;
	}

	sops.sem_flg = 0;
	sops.sem_num = 0;
	sops.sem_op = 0;
	semop(s_id, &sops, 1);


	clock_gettime(CLOCK_REALTIME, &start);


	while(1){
		/*Ricalcolo il bilancio*/
		bilancio = calcolaBilancio(libro_mastro);

	  	msgctl(m_id, IPC_STAT, &buf);
	  	num_messages = buf.msg_qnum;

		if(bilancio >= 2 && num_messages < 100){
			current_retry = 0;
			inviaTransazione();
			
		}else if(bilancio < 2){
			current_retry++;

			if(current_retry >= SO_RETRY){ /* L'utente non ha avuto li bilancio necessario per l'invio di una transazione per SO_RETRY volte ---> termina */

				do{
					sops.sem_op = -1;
					sops.sem_num = 3;
				}while(semop(s_id, &sops, 1) == -1);

				users_shared->nUsers--;

				if(users_shared->nUsers <= 1){ /* Resta solo più un utente attivo --> invio al master segnale SIGUSR2 */
					users_shared->nUsers--;
					kill(getppid(), SIGUSR2);
				}

				sops.sem_op = 1;
				sops.sem_num = 3;
				semop(s_id, &sops, 1);

				exit(EXIT_SUCCESS);
			}
		}
		
		/* --- attesa di un tempo in nanosecondi compreso tra SO_MIN_TRANS_GEN_NSEC e SO_MAX_TRANS_GEN_NSEC ---*/
		tim.tv_sec = 0;
		tim.tv_nsec = (rand()%SO_MAX_TRANS_GEN_NSEC-SO_MIN_TRANS_GEN_NSEC+1)+SO_MIN_TRANS_GEN_NSEC;
		
		if(nanosleep(&tim, NULL) < 0 ){}
	 }

	exit(EXIT_SUCCESS);
}

void inviaTransazione(){
	int j, q, receiverIndex;
	struct transazione nuovaTransazione;

	/* --- viene scelto l'utente a cui inviare una transazione e vengono aggiunte le altre informazioni ---*/

	receiverIndex = getReceiverIndex(users_shared->users);

	clock_gettime(CLOCK_REALTIME, &stop);

	nuovaTransazione.timestamp = stop.tv_nsec - start.tv_nsec;
	nuovaTransazione.sender = getpid();
	nuovaTransazione.receiver = users_shared->users[receiverIndex];

	q = (rand()%bilancio) + 1;

	if(q < 5){
		nuovaTransazione.reward = 0;
	}else{
		nuovaTransazione.reward = (q * ((float)SO_REWARD / 100));
	}
	nuovaTransazione.quantity = q - nuovaTransazione.reward;
	
	/* --- invio della transazione al nodo scelto ---*/
	msg_t.mtype = nodes_shared->nodes[getNodeIndex()];
	msg_t.tr_msg = nuovaTransazione;
	msgsnd(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), 0);

	lastSent[lsIndex] = nuovaTransazione;
	j = 0;

	do{
		lsIndex = (lsIndex+1)%(LASTSENTSIZE);

		j++;
		if(j >= LASTSENTSIZE){
			sleep(1);
			j = 0;
		}
	}while(lastSent[lsIndex].timestamp != -1);
}

int calcolaBilancio(struct blocco *libro_mastro){
	int i, budget = 0;

	struct blocco *libro = libro_mastro;

	bilancioMastro += calcolaBilancioMaster();

	for(i = 0; i < LASTSENTSIZE; i++){
		if(lastSent[i].timestamp != -1){
			budget -= (lastSent[i].quantity + lastSent[i].reward);
		}
	}

	return budget + bilancioMastro;
}

int calcolaBilancioMaster(){
	int bilancioM = 0, j, k, i;

	for(i = lastIndexI; i < SO_REGISTRY_SIZE; i++){
		if(libro_mastro[i].id != -1){
			for(j = 0; j < SO_BLOCK_SIZE - 1; j++){ /* SO_BLOCK_SIZE - 1 perchè l'ultimo elemento di ogni blocco è sempre il guadagno di un nodo */
				
				if(libro_mastro[i].t[j].sender == getpid()){

					for(k = 0; k < LASTSENTSIZE; k++){
						if(lastSent[k].timestamp == libro_mastro[i].t[j].timestamp  &&  lastSent[k].receiver == libro_mastro[i].t[j].receiver){
							lastSent[k].timestamp = -1;
						}
					}

					bilancioM -= (libro_mastro[i].t[j].quantity + libro_mastro[i].t[j].reward);

				}else if(libro_mastro[i].t[j].receiver == getpid()){
					bilancioM += libro_mastro[i].t[j].quantity;
				}
			}
		}else{
			break;
		}
	}

	lastIndexI = i;
	return bilancioM;
}

int getReceiverIndex(pid_t *user_pids){
	int receiverIndex, exist;

	do{
		receiverIndex = rand()%SO_USER_NUM;
		exist = kill(user_pids[receiverIndex], 0);
	}while(user_pids[receiverIndex] == getpid() || exist == -1);
	
	return receiverIndex;
}

int getNodeIndex(){
	int	nodeIndex = rand()%nodes_shared->nNodes;
	
	return nodeIndex;
}
