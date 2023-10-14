#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <errno.h>
#include <string.h>
#include <time.h>
#include <sys/stat.h>

#define KEY 1234

#ifndef PARAMETERS_H
#define PARAMETERS_H

extern int SO_USER_NUM;
extern int SO_NODES_NUM;
extern int SO_BUDGET_INIT;
extern int SO_REWARD;
extern long int SO_MIN_TRANS_GEN_NSEC;
extern long int SO_MAX_TRANS_GEN_NSEC;
extern int SO_RETRY;
extern int SO_TP_SIZE;
extern long int SO_MIN_TRANS_PROC_NSEC;
extern long int SO_MAX_TRANS_PROC_NSEC;
extern int SO_SIM_SEC;
extern int SO_NUM_FRIENDS;
extern int SO_HOPS;

extern int SO_MAX_NODE_NUM;

#endif

#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 10000

/*
Conf 1:
#define SO_BLOCK_SIZE 100
#define SO_REGISTRY_SIZE 1000

Conf 2:
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 10000

Conf 3:
#define SO_BLOCK_SIZE 10
#define SO_REGISTRY_SIZE 1000

*/




struct transazione{
	int timestamp;
	pid_t sender; /* pid_t ?*/
	pid_t receiver; /* pid_t ?*/
	int quantity;
	int reward;
};

struct blocco{
	int id;
	struct transazione t[SO_BLOCK_SIZE];
};

struct users_shm {
	int nUsers;
	pid_t users[4000];
};

struct nodes_shm {
	int nNodes;
	pid_t nodes[4000];
};

struct msg_transazione {
	long mtype;             /* message type, must be > 0 */
	struct transazione tr_msg;    /* message data */
	int flag; /*se:
				0: transazione inviata da un utente
				>0: transazione inviata da un nodo (il numero e il suo pid)
				*/
	int n_hop;
};

struct msg_friends {
	long mtype;
	pid_t pid_f[4000];
};
