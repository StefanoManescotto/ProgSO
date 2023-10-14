/*
	*Nodo
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

#define SENDER_NODE -1 /* Valore che viene messo nel campo "sender" del libro mastro nella transazione del guadagno del nodo */
#define SEND_NODE 10 /* ogni 10 transazioni ricevute ne estrae una dalla tp e la invia ad un altro nodo */

void addBlock(struct transazione *);
pid_t getfriend(pid_t);

int b_index = 0, tp_index = 0, *block_index, s_id, m_id, nFriends;
pid_t *friends = NULL;
struct transazione *transactionPool = NULL;
struct blocco *libro_mastro;
struct timespec tim;
struct sembuf sops;
struct timespec start, stop;
struct msg_friends msg_f;
struct msg_transazione msg_t;
struct nodes_shm *nodes_shared;

struct sigaction sa, sa2;
sigset_t  my_mask, my_mask2;


void signalHandler(int sig){
	switch(sig){
		case SIGUSR1:

			msgrcv(m_id, &msg_f, sizeof(pid_t)*(SO_NUM_FRIENDS), 1, 0);

			friends[nFriends] = msg_f.pid_f[0];
			nFriends++;
			break;
		case SIGINT:
			printf("nodo %d finito. La tp ha ancora %d elementi\n", getpid(), tp_index);
			exit(EXIT_SUCCESS);
	}
}

void freeExit(){
	if(friends != NULL){
		free(friends);
		friends = NULL;
	}
	if(transactionPool != NULL){
		free(transactionPool);
		transactionPool = NULL;
	}
}


int main(int argc, char* argv[]){
    int i = 0, j = 0, bytes_received = 0, received_num = 0, n, t = 0;
    struct transazione blocco[SO_BLOCK_SIZE];
   
    if(definisci() == -1){
		printf("valori inseriti errati\n");
		exit(EXIT_FAILURE);
	}

	atexit(freeExit);

    nFriends = SO_NUM_FRIENDS;

    transactionPool = malloc(sizeof(struct transazione) * SO_TP_SIZE);
    friends = malloc(sizeof(pid_t) * SO_NUM_FRIENDS);

    sigemptyset(&my_mask);
	sigaddset(&my_mask, SIGUSR2);
	sigaddset(&my_mask, SIGTERM);

	sa.sa_mask = my_mask;
	sa.sa_handler = signalHandler;
	sa.sa_flags = SA_RESTART;
	sigaction(SIGUSR1, &sa, NULL);

    sigemptyset(&my_mask2);
	sigaddset(&my_mask2, SIGUSR1);
	sigaddset(&my_mask2, SIGUSR2);

	sa2.sa_mask = my_mask2;
	sa2.sa_handler = signalHandler;
	sa2.sa_flags = 0;
	sigaction(SIGINT, &sa2, NULL);
	
	s_id = atoi(argv[3]);

	m_id = msgget(KEY, IPC_CREAT | 0600);
	nodes_shared = shmat(atoi(argv[4]), NULL, 0);

	sops.sem_op = 0;
	sops.sem_num = 0;
	sops.sem_flg = 0;
	semop(s_id, &sops, 1);

	semctl(s_id, 2, SETVAL, 1);

	msgrcv(m_id, &msg_f, sizeof(pid_t)*(SO_NUM_FRIENDS), getpid(), 0);

	memcpy(friends, msg_f.pid_f, sizeof(pid_t)*(SO_NUM_FRIENDS));

	libro_mastro = shmat(atoi(argv[1]), NULL, 0);
	block_index = shmat(atoi(argv[2]), NULL, 0);

	clock_gettime(CLOCK_REALTIME, &start);


	bytes_received = 0;
	b_index = 0;

	while(1){
		do{
			if(tp_index < SO_BLOCK_SIZE - 2){
				bytes_received = msgrcv(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), getpid(), 0);
			}else{
				bytes_received = msgrcv(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), getpid(), IPC_NOWAIT);
			}
			
			if(bytes_received != -1){

				if(tp_index < SO_TP_SIZE){
					transactionPool[tp_index] = msg_t.tr_msg;
					tp_index++;
					received_num++;

					if(received_num >= SEND_NODE){
						received_num = 0;

						n = rand()%tp_index;
						
						msg_t.n_hop = 1;
						msg_t.flag = getpid();
						msg_t.mtype = getfriend(msg_t.flag);
						msg_t.tr_msg = transactionPool[n];
						
						msgsnd(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), 0);

						for(i = n; i + 1 < tp_index; i++){
							transactionPool[i] = transactionPool[i+1];
						}

						tp_index--;
					}
				}else{ /* Transaction Pool piena*/
					if(msg_t.flag == 0 || nodes_shared->nNodes >= SO_MAX_NODE_NUM){
						msg_t.mtype = msg_t.tr_msg.sender;

						msgsnd(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), 0);

						kill(msg_t.tr_msg.sender, SIGUSR1);
					}else{
						if(msg_t.n_hop >= SO_HOPS && nodes_shared->nNodes < SO_MAX_NODE_NUM){
							do{
								sops.sem_flg = 0;
								sops.sem_num = 2;
								sops.sem_op = -1;
								t = semop(s_id, &sops, 1);
							}while(t == -1);

							msg_t.flag = getpid();
							msg_t.mtype = getppid();
							msgsnd(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), 0);

							kill(getppid(), SIGUSR1);

						}else{
							msg_t.mtype = getfriend(msg_t.flag);
							msg_t.flag = getpid();
							msg_t.n_hop++;

							msgsnd(m_id, &msg_t, sizeof(struct transazione) + (sizeof(int) * 2), 0);
						}
					}
				}
			}
			
		}while(bytes_received != -1);

		if(tp_index > SO_BLOCK_SIZE - 2){
			i = 0;
			b_index = 0;

			while(i <= SO_BLOCK_SIZE - 2){
				blocco[b_index] = transactionPool[i];
				b_index++;
				i++;
			}

			addBlock(blocco);

			b_index = 0;
			tim.tv_sec = 0;

			/* --- simulazione elaborazione blocco --- */
			tim.tv_nsec = (rand()%SO_MAX_TRANS_PROC_NSEC - SO_MIN_TRANS_PROC_NSEC + 1) + SO_MIN_TRANS_PROC_NSEC;
			if(nanosleep(&tim, NULL) < 0 ){}


			/* --- elaborazione completata --> aggiornamento della transaction pool */
			j = 0;
			i = SO_BLOCK_SIZE - 1;

			for(i; i < tp_index; i++){
				transactionPool[j] = transactionPool[i];
				j++;
			}

			tp_index = j;
		}
	}
    
    exit(EXIT_SUCCESS);
}

/* Aggiunge un blocco all'interno del libro mastro */
void addBlock(struct transazione *b){
	int t;

	clock_gettime(CLOCK_REALTIME, &stop);

	b[SO_BLOCK_SIZE - 1].timestamp = stop.tv_nsec - start.tv_nsec;
	b[SO_BLOCK_SIZE - 1].sender = SENDER_NODE;
	b[SO_BLOCK_SIZE - 1].receiver = getpid();
	b[SO_BLOCK_SIZE - 1].quantity = calcReward(b);
	b[SO_BLOCK_SIZE - 1].reward = 0;

	do{
		sops.sem_op = -1;
		sops.sem_num = 1;
	}while(semop(s_id, &sops, 1) == -1);

	memcpy(libro_mastro[*block_index].t, b, sizeof(struct transazione) * SO_BLOCK_SIZE);

	libro_mastro[*block_index].id = (*block_index);

	if((*block_index)+1 >= SO_REGISTRY_SIZE){
		do{
			sops.sem_flg = 0;
			sops.sem_num = 2;
			sops.sem_op = -1;
			t = semop(s_id, &sops, 1);
		}while(t == -1);

		kill(getppid(), SIGTERM);
	}else{
		(*block_index)++;
	}

	sops.sem_op = 1;
	semop(s_id, &sops, 1);
}

/* Calcola quanti soldi il nodo riceverà per l'elaborazione del blocco */
int calcReward(struct transazione *b){
	int i, reward = 0;
	for(i = 0; i < SO_BLOCK_SIZE - 1; i++){
		reward += b[i].reward;
	}

	return reward;
}

/* Estrae un nodo amico casuale diverso da "last" */
pid_t getfriend(pid_t last){
	int i;

	do{
		i = rand()%nFriends;
	}while(friends[i] == last);

	return friends[i];
}
