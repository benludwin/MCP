#define _GNU_SOURCE

#include <sys/wait.h>
#include <sys/time.h>
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
int ProgramCount = 0;
int CurrentProgram = 0;
int StartRun = 0;
int Exit = 0;

#define TIMER_INTERVAL 100

enum State
{
	NotStarted,
	Running,
	Paused,
	Exited
};

struct Program {
	char *command;
	char **args;
	pid_t PID;
	enum State state;
	int num_args;
	int HasExited;
	int utime;
	int stime;
};

int CheckAllHasExited(struct Program *PCB){
	int result = 1;
	for (int i = 0; i < ProgramCount; i++){
		if (PCB[i].state != Exited){
			result = 0;
		}
	}
	return result;
}

int Advance(int program){
	for (int i = program+1; i < ProgramCount; i++){
		if (PCB[i].state != Exited){
			return i;
		}
	}
	for (int i = 0; i < program; i++){
		if (PCB[i].state != Exited){
			return i;
		}
	}
	return program;
}

void SIGALARM(int signal){
	if (PCB[CurrentProgram].HasExited == 1){
		PCB[CurrentProgram].state = Exited;
	} else if (PCB[CurrentProgram].state != NotStarted){
		PCB[CurrentProgram].state = Paused;
		kill(PCB[CurrentProgram].PID, SIGSTOP);
	}
	if (CheckAllHasExited(PCB) == 1){
		raise(SIGUSR2);
	}
	CurrentProgram = Advance(CurrentProgram);
	
	if (PCB[CurrentProgram].state == NotStarted){
		PCB[CurrentProgram].state = Running;
		kill(PCB[CurrentProgram].PID, SIGUSR1);
	} else {
		PCB[CurrentProgram].state = Running;
		kill(PCB[CurrentProgram].PID, SIGCONT);
	}
}

void SIGCHILD(int signal){
	int status;
	for (int i = 0; i < ProgramCount; i++){
		if (waitpid(PCB[i].PID, &status, WNOHANG) > 0){
			if (WIFEXITED(status) && PCB[i].state != NotStarted){
				PCB[i].HasExited = 1;
			}
		}		
	}
}

void SIGUSRI(){
	StartRun = 1;	
}

void SIGUSRII(){
	Exit = 1;
}

int GetNumLines(FILE* input){
	int count = 0;
	for (int i = getc(input); i != EOF; i = getc(input)){
		if (i== '\n'){
			count += 1;	
		}
	}
	fseek(input, 0, SEEK_SET);
	return count;
}

int WaitSignal(int signal){
	int sig;
	sigset_t sigset;
	sigemptyset(&sigset);
	sigaddset(&sigset, signal);
	sigprocmask(SIG_BLOCK, &sigset, NULL);

	return sigwait(&sigset, &sig);
}

void WaitForAllExit(struct Program *PCB){
	fprintf(stderr, "Wait Called\n");
	
	sigset_t mask, oldmask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGUSR2);
	sigprocmask (SIG_BLOCK, &mask, &oldmask);
	while (Exit==0){
		sigsuspend (&oldmask);
	}
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
}

void FreePrograms(struct Program *PCB){
	for (int i = 0; i < ProgramCount; i++){
                free(PCB[i].command);
                for (int j = 0; j <= PCB[i].num_args; j++){
                        free(PCB[i].args[j]);
                }
                free(PCB[i].args);
	}
	free(PCB);
}

void LaunchAllPrograms(struct Program *PCB){
	int err;
	pid_t parentPID = getpid();
	for (int i = 0; i < ProgramCount; i++){
		signal(SIGUSR1, SIGUSRI);
		PCB[i].PID = fork();

		if (PCB[i].PID < 0){
			fprintf(stderr, "Failed to start process number: %d", i);
		}
		else if (PCB[i].PID == 0){
			while (StartRun == 0){
				usleep(100);
			}
                        err = execvp(PCB[i].command, PCB[i].args);
                        if (err == -1){
                                fprintf(stderr, "Failed execvp\n");
                                PCB[i].state = Exited;
                                PCB[i].HasExited = 1;
                        }
                        FreePrograms(PCB);
                        fclose(input);
                        _exit(0);
		}
	}
}

void SendAllProgramsSignal(struct Program *PCB, int sig){
	int err;
	for (int i = 0; i < ProgramCount; i++){
		err = kill(PCB[i].PID, sig);
	}
}

struct Program * MallocPrograms(FILE *input){
	struct Program *PCB = (struct Program *)malloc(sizeof(struct Program)*ProgramCount);
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
		PCB[i].PID = -1;
		PCB[i].state = NotStarted;
		PCB[i].HasExited = 0;
		i++;
	}
	return PCB;
}

void Scheduler(struct Program* PCB){
	signal(SIGALRM, SIGALARM);
	signal(SIGCHLD, SIGCHILD);
	signal(SIGUSR2, SIGUSRII);
	struct itimerval it_val;
	
	it_val.it_value.tv_sec = 0;
	it_val.it_value.tv_usec = 100 * 1000;
	it_val.it_interval = it_val.it_value;
	setitimer(ITIMER_REAL, &it_val, NULL);
	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		perror("setitimer");
		exit(1);
	}
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
	ProgramCount = GetNumLines(input);	
	CurrentProgram = ProgramCount-1;
	PCB = MallocPrograms(input);
	
	LaunchAllPrograms(PCB);
	Scheduler(PCB);
	WaitForAllExit(PCB);
	FreePrograms(PCB);
	return fclose(input);
}
