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

#define BUFFER_SIZE 4096

int SO_USER_NUM;
int SO_NODES_NUM;
int SO_BUDGET_INIT;
int SO_REWARD;
long int SO_MIN_TRANS_GEN_NSEC;
long int SO_MAX_TRANS_GEN_NSEC;
int SO_RETRY;
int SO_TP_SIZE;
long int SO_MIN_TRANS_PROC_NSEC;
long int SO_MAX_TRANS_PROC_NSEC;
int SO_SIM_SEC;
int SO_NUM_FRIENDS;
int SO_HOPS;

int SO_MAX_NODE_NUM;


int definisci(){
	FILE *f = fopen("./Configurazioni/configTxt.txt", "r");
	
	/*
	
	FILE *f = fopen("./Configurazioni/configUtenti.txt", "r");
	FILE *f = fopen("./Configurazioni/configLibro.txt", "r");
	FILE *f = fopen("./Configurazioni/configTempo.txt", "r");
	
	FILE *f = fopen("./Configurazioni/config1.txt", "r");

	*/

	char text[BUFFER_SIZE];

	char *line, *key;
	long int value;

	fread(text, 1, BUFFER_SIZE, f);

	line = strtok(strdup(text), "\n");

	while(line){
		key = strsep(&line, "=");
		key[strlen(key)-1] = '\0';
		value = atol(strsep(&line, "="));

		if(strcmp(key, "SO_USER_NUM") == 0){
			if(value >= 2  && value <= 2000){
				SO_USER_NUM = value;
			}else{
				return -1;
			}
		}else if(strcmp(key, "SO_NODES_NUM") == 0){
			if(value >= 2  && value <= 200){
					SO_NODES_NUM = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_BUDGET_INIT") == 0){
			if(value > 2){
				SO_BUDGET_INIT = value;
			}else{
				return -1;
			}
		}else if(strcmp(key, "SO_REWARD") == 0){
			if(value >= 0 && value < 100){
				SO_REWARD = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_MIN_TRANS_GEN_NSEC") == 0){
			if(value >= 0 && value <= 999999999){
				SO_MIN_TRANS_PROC_NSEC = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_MAX_TRANS_GEN_NSEC") == 0){
			if(value >= 0 && value <= 999999999){
				SO_MAX_TRANS_GEN_NSEC = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_RETRY") == 0){
			if(value > 0){
				SO_RETRY = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_TP_SIZE") == 0){
			if(value > 0 && value > SO_BLOCK_SIZE){
				SO_TP_SIZE = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_MIN_TRANS_PROC_NSEC") == 0){
			if(value >= 0 && value <= 999999999){
				SO_MIN_TRANS_PROC_NSEC = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_MAX_TRANS_PROC_NSEC") == 0){
			if(value >= 0 && value <= 999999999){
				SO_MAX_TRANS_PROC_NSEC = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_SIM_SEC") == 0){
			if(value > 0){
				SO_SIM_SEC = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_NUM_FRIENDS") == 0){
			if(value > 0){
				SO_NUM_FRIENDS = value;
			}else{
				return -1;
			}
		}
		else if(strcmp(key, "SO_HOPS") == 0){
			if(value > 0){
				SO_HOPS = value;
			}else{
				return -1;
			}
		}

		line = strtok(NULL, "\n");
	}

	if(SO_MIN_TRANS_PROC_NSEC > SO_MAX_TRANS_PROC_NSEC){
		return -1;
	}

	if(SO_MIN_TRANS_GEN_NSEC > SO_MAX_TRANS_GEN_NSEC){
		return -1;
	}

	if(SO_NUM_FRIENDS > SO_NODES_NUM){
		return -1;
	}

	SO_MAX_NODE_NUM = SO_NODES_NUM * 2;

    return 0;
}
