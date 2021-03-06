#define _GNU_SOURCE

#include <sys/wait.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
/*
 * BenLudwin, 951510335, CIS415 Project 1
 * This is my own work.
 */
struct Program *PCB = NULL;
FILE* input = NULL;	
int StartRun = 0;

struct Program {
	char *command;
	char **args;
	pid_t PID;
	int num_args;
};

void SIGUSRI(){
        StartRun = 1;
}

int getNumLines(FILE* input){
	int Num_Lines = 0;
	for (int i = getc(input); i != EOF; i = getc(input)){
		if (i== '\n'){
			Num_Lines += 1;	
		}
	}
	fseek(input, 0, SEEK_SET);
	return Num_Lines;
}

int waitSignal(int signal){
	int sig = 0;
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, signal);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	return sigwait(&sigset, &sig);
}

void WaitForAllPrograms(struct Program *PCB, int num_programs){
	for (int i = 0; i <= num_programs - 1; i++){
		waitpid(PCB[i].PID, NULL, WUNTRACED);
	}
}

void FreePrograms(struct Program *PCB, int num_programs){
	for (int i = 0; i <= num_programs -1; i++){
		free(PCB[i].command);
		for (int j = 0; j <= PCB[i].num_args; j++){
			free(PCB[i].args[j]);
		}
		free(PCB[i].args);
	}
	free(PCB);
}

void LaunchAllPrograms(struct Program *PCB, int num_programs){
	int err;
	pid_t parentPID = getpid();
	for (int i = 0; i <= num_programs-1; i++){
                signal(SIGUSR1, SIGUSRI);
		PCB[i].PID = fork();

		if (PCB[i].PID < 0){
			perror("Failed Fork");
		}
		else if (PCB[i].PID == 0){
                        while (StartRun == 0){
                                usleep(100);
                        }
                        err = execvp(PCB[i].command, PCB[i].args);
                        if (err == -1){
				perror("Failed execvp");
                        }
                        FreePrograms(PCB, num_programs);
                        fclose(input);
                        _exit(0);
		}
	}
}

void SendAllProgramsSignal(struct Program *PCB, int num_programs, int sig){
	int err;
	for (int i = 0; i <= num_programs - 1; i++){
		err = kill(PCB[i].PID, sig);
	}
}

struct Program * mallocPrograms(FILE *input, int num_programs){
	struct Program *PCB = (struct Program *)malloc(sizeof(struct Program)*num_programs);
	char line[64];
	int i = 0;
	while (fgets(line, sizeof(line), input) != NULL){
		char * p = strtok(line, " \t\n");
		char ** arguments = (char **)malloc(sizeof(char *)*16);
		char ** temp;
		
		PCB[i].command = strdup(p);		
		arguments[0] = strdup(p);
		p = strtok(NULL, " \t\n");
		int j = 1;
		
		while (p != NULL){
			arguments[j] = strdup(p);
			p = strtok(NULL, " \t");
			j++;
		}
		arguments[j] = NULL;
		temp = (char **)realloc(arguments, (j+1)*sizeof(char *));
		if (temp != NULL){ arguments = temp; }
		PCB[i].args = arguments;
		PCB[i].num_args = j - 1;
		i++;
	}
	return PCB;
}

int main(int argc, char* argv[]){
        if (argc < 2){
                perror("Bad Arguments");
                exit(1);
        }
        if (argc == 2){ input = fopen(argv[1], "r"); }
        if (input == NULL){
                perror("Bad Input File");
                exit(1);
        }

	int num_programs = getNumLines(input);	
	PCB = mallocPrograms(input, num_programs);
	
	LaunchAllPrograms(PCB, num_programs);

	SendAllProgramsSignal(PCB,num_programs,SIGUSR1);
	SendAllProgramsSignal(PCB,num_programs,SIGSTOP);
	SendAllProgramsSignal(PCB,num_programs,SIGCONT);

	WaitForAllPrograms(PCB, num_programs);
	
	FreePrograms(PCB, num_programs);	
	return fclose(input);
}
