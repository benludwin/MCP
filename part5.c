#define _GNU_SOURCE

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
/*
 * Ben Ludwin, 951510335, CIS415 Project 1
 * This is my own work except WaitForAllExit and UpdateAndPrintProcInfo were provided by Roscoe.
 */

struct Program *PCB = NULL;
FILE* input = NULL;	
int ProgramCount = 0;
int ExitedCount = 0;
int CurrentProgram = 0;
int StartRun = 0;
int Exit = 0;
struct itimerval it_val;

void SIGUSRI(){ StartRun = 1; }

void SIGUSRII(){ Exit = 1; }

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
	int HasExited;
	int TimeToSchedule;
	int num_args;
	int utime;
	int stime;
};

void InitProgram(struct Program *PCB){
	PCB->PID = -1;
	PCB->state = NotStarted;
	PCB->HasExited = 0;
	PCB->utime = 0;
	PCB->stime = 0;
	PCB->TimeToSchedule = 100;
}

FILE *GetStatsFile(pid_t PID){
	FILE *file;
	char filename[128];
	snprintf(filename, sizeof(filename), "/proc/%u/stat", PID);
	file = fopen(filename, "r");
	if (file == NULL) {
		perror("No stat file available");
	}
	return file;
}

void UpdateAndPrintProcInfo(struct Program *PCB){
	FILE *file = GetStatsFile(PCB->PID);
	if(file!=NULL){
		char buffer[1024];
		char delimiters[] = " ";
		fgets(buffer, sizeof(buffer), file);
		int utime = 0; int stime = 0;
		int i = 1;
		char *ptr = strtok(buffer, delimiters);
		do{
			if (i == 14){ utime = atoi(ptr); }
			if (i == 15){ stime = atoi(ptr); }
			i++;
			ptr = strtok(NULL, delimiters);
		} while (ptr!=NULL);
		fclose(file);
		PCB->utime = utime;
		PCB->stime = stime;
		fprintf(stderr, "Process %d\n\tutime - %d\n\tstime - %d\n\tTimeToSchedule - %d\n", PCB->PID, PCB->utime, PCB->stime, PCB->TimeToSchedule);
	}
}

void UpdateTimeToSchedule(){
	double div;
	int time;
	for (int i = 0; i < ProgramCount; i++){
		time = PCB[i].utime + PCB[i].stime;
		if (PCB[i].state != Exited && time <= 0){
			return;
		}
	}
	for (int i = 0; i < ProgramCount; i++){
		if (PCB[i].state != Exited){
			div = (double)(PCB[i].utime) / (PCB[i].utime + PCB[i].stime);
			PCB[i].TimeToSchedule = (int)(10.0 + (div * 90));
		}
	}
}

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

void SetTimer(int ms_interval){
	it_val.it_value.tv_sec = 0;
	it_val.it_value.tv_usec = ms_interval * 1000;  
	it_val.it_interval = it_val.it_value;
	if (setitimer(ITIMER_REAL, &it_val, NULL) == -1) {
		perror("setitimer");
		exit(1);
	}
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
	if (PCB[CurrentProgram].HasExited != 1){
		UpdateAndPrintProcInfo(&PCB[CurrentProgram]);
	}
	CurrentProgram = Advance(CurrentProgram);
	
	if (PCB[CurrentProgram].state == NotStarted){
		PCB[CurrentProgram].state = Running;
		UpdateTimeToSchedule();
		kill(PCB[CurrentProgram].PID, SIGUSR1);
		SetTimer(PCB[CurrentProgram].TimeToSchedule);
	} else {
		PCB[CurrentProgram].state = Running;
		UpdateTimeToSchedule();
		kill(PCB[CurrentProgram].PID, SIGCONT);
		SetTimer(PCB[CurrentProgram].TimeToSchedule);
	}
}

void SIGCHILD(int signal){
	int status;
	for (int i = 0; i < ProgramCount; i++){
		if (waitpid(PCB[i].PID, &status, WNOHANG) > 0){
			if (WIFEXITED(status)){
				PCB[i].HasExited = 1;
				ExitedCount++;
			}
		}		
	}
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
			p = strtok(NULL, " \t\n");
			j++;
		}
		arguments[j] = NULL;
		temp = (char **)realloc(arguments, (j+1)*sizeof(char *));
		if (temp != NULL){ arguments = temp; }
		PCB[i].args = arguments; 
		PCB[i].num_args = j - 1;
		InitProgram(&PCB[i]);
		i++;
	}
	return PCB;
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
                        perror("Failed Fork");
                }
                else if (PCB[i].PID == 0){
                        while (StartRun == 0){
                                usleep(100);
                        }
                        err = execvp(PCB[i].command, PCB[i].args);
                        if (err == -1){
                                perror("Failed execvp");
                                PCB[i].state = Exited;
                                PCB[i].HasExited = 1;
                        }
                        FreePrograms(PCB);
                        fclose(input);
                        _exit(0);
                }
        }
}

void InitSignalHandlers(){
	signal(SIGALRM, SIGALARM);
	signal(SIGCHLD, SIGCHILD);
	signal(SIGUSR2, SIGUSRII);
}

void WaitForAllExit(struct Program *PCB){
	sigset_t mask, oldmask;
	sigemptyset (&mask);
	sigaddset (&mask, SIGUSR2);
	sigprocmask (SIG_BLOCK, &mask, &oldmask);
	while (Exit==0){
		sigsuspend (&oldmask);
	}
	sigprocmask (SIG_UNBLOCK, &mask, NULL);
}

void Scheduler(struct Program* PCB){
	LaunchAllPrograms(PCB);
	InitSignalHandlers();
	SetTimer(100);	
	WaitForAllExit(PCB);
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
	CurrentProgram = ProgramCount - 1;
	PCB = MallocPrograms(input);	
	Scheduler(PCB);
	FreePrograms(PCB);
	return fclose(input);
}
