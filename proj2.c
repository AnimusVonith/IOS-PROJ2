#include <stdio.h>
#include <sys/types.h> 
#include <unistd.h> //fork(), usleep()
#include <stdlib.h> //exit(), strtol()
#include <semaphore.h> //semaphore
#include <sys/mman.h> //shared memory
#include <string.h> //strcmp()
#include <sys/stat.h>
#include <fcntl.h> //O_CREAT, O_EXCL
#include <errno.h> //errno
#include <limits.h> //INT_MAX

sem_t* semaphore_main = NULL;
sem_t* semaphore_imm_gen = NULL;
sem_t* semaphore_write = NULL;
sem_t* semaphore_judgement = NULL;
sem_t* semaphore_entry = NULL;
sem_t* semaphore_exit = NULL;
sem_t* semaphore_registration = NULL;
sem_t* semaphore_mutex = NULL;


int* PI;	//n of imigrants
int* IG;	//max delay - imigrant generovanie
int* JG;	//max delay - sudca vstupi do budovy
int* IT;	//max delay - imigrant vyzdvihnutie certifikat
int* JT;	//max delay - sudca vydava certifikaty
int* A;  	//poradie vykonania
int* NE; 	//imigranti - vstupili do budovy a nebolo rozhodnute
int* NB; 	//imigranti - zaregistrovali a nebolo rozhodnute
int* NC; 	//imigranti - v budove celkovo
int* JIDH;	//judge in da house 0 - false, 1 - true
	//IG, JG, IT, JT = <0, 2000>

FILE* write_file = NULL;

void clean_exit(int exit_code) {

	munmap(PI, sizeof(int));
	munmap(IG, sizeof(int));
	munmap(JG, sizeof(int));
	munmap(IT, sizeof(int));
	munmap(JT, sizeof(int));
	munmap(A, sizeof(int));
	munmap(NE, sizeof(int));
	munmap(NB, sizeof(int));
	munmap(NC, sizeof(int));
	munmap(JIDH, sizeof(int));

	sem_close(semaphore_main);
	sem_close(semaphore_imm_gen);
	sem_close(semaphore_write);
	sem_close(semaphore_judgement);
	sem_close(semaphore_entry);
	sem_close(semaphore_exit);
	sem_close(semaphore_registration);
	sem_close(semaphore_mutex);
	sem_unlink("/xadamc07.sem_main_end");
	sem_unlink("/xadamc07.sem_imm_gen");
	sem_unlink("/xadamc07.sem_write_to_file");
	sem_unlink("/xadamc07.sem_judgement");
	sem_unlink("/xadamc07.sem_entry");
	sem_unlink("/xadamc07.sem_exit");
	sem_unlink("/xadamc07.sem_reg");
	sem_unlink("/xadamc07.mutex");
	exit(exit_code);
}

void* createSharedMemory(size_t size){
	int protection = PROT_READ | PROT_WRITE;
	int visibility = MAP_ANONYMOUS | MAP_SHARED;
	return mmap(NULL, size, protection, visibility, -1, 0);
}

int get_random_number(int max_n) {
	if(max_n == 0){
		return 0;
	}
	return (rand() % (max_n + 1));
}

void writeToFile(char* NAME, char* msg, int I) {
	sem_wait(semaphore_write);
	write_file = fopen("./proj2.out", "a+");
	if (I != 0){ //IMMs
		if(strcmp(msg, "starts") == 0){
			fprintf(write_file, "%2d: %s %d: %s\n", *A, NAME, I, msg);
		} else {
			fprintf(write_file, "%2d: %s %d: %s: %d: %d: %d\n", *A, NAME, I, msg, *NE, *NC, *NB);
		}
		
	}else{ //JUDGE
		if (strcmp(msg, "wants to enter") == 0 || strcmp(msg, "finishes") == 0){
			fprintf(write_file, "%2d: %s: %s\n", *A, NAME, msg);
		} else {
			fprintf(write_file, "%2d: %s: %s: %d: %d: %d\n", *A, NAME, msg, *NE, *NC, *NB);
		}
		
	}
	fclose(write_file);
	*A=*A+1;
	sem_post(semaphore_write);
}

void init() {
	if((write_file = fopen("proj2.out", "w+")) == NULL){                        
        fprintf(stderr, "Nepodarilo sa otvorit subor pre zapisovanie vysledkov!\n");
        exit(1);
    }
    fclose(write_file);

    A = (int*)createSharedMemory(sizeof(int));
    *A = 1;
    NE = (int*)createSharedMemory(sizeof(int));
    *NE = 0;
	NB = (int*)createSharedMemory(sizeof(int));
	*NB = 0;
	NC = (int*)createSharedMemory(sizeof(int));
	*NC = 0;
	JIDH = (int*)createSharedMemory(sizeof(int));
	*JIDH = 0;

    PI = (int*)createSharedMemory(sizeof(int));
    *PI = 5;
	IG = (int*)createSharedMemory(sizeof(int));
	*IG = 0;
	JG = (int*)createSharedMemory(sizeof(int));
	*JG = 0;
	IT = (int*)createSharedMemory(sizeof(int));
	*IT = 0;
	JT = (int*)createSharedMemory(sizeof(int));
	*JT = 0;


	if	(
		( (semaphore_main = sem_open("/xadamc07.sem_main_end", O_CREAT | O_EXCL, 0666, 0) ) == SEM_FAILED ) ||
		( (semaphore_imm_gen = sem_open("/xadamc07.sem_imm_gen", O_CREAT | O_EXCL, 0666, 0) ) == SEM_FAILED )  ||
		( (semaphore_write = sem_open("/xadamc07.sem_write_to_file", O_CREAT | O_EXCL, 0666, 1) ) == SEM_FAILED ) ||
		( (semaphore_judgement = sem_open("/xadamc07.sem_judgement", O_CREAT | O_EXCL, 0666, 0) ) == SEM_FAILED ) ||
		( (semaphore_entry = sem_open("/xadamc07.sem_entry", O_CREAT | O_EXCL, 0666, 1) ) == SEM_FAILED ) ||
		( (semaphore_exit = sem_open("/xadamc07.sem_exit", O_CREAT | O_EXCL, 0666, 0) ) == SEM_FAILED ) ||
		( (semaphore_registration = sem_open("/xadamc07.sem_reg", O_CREAT | O_EXCL, 0666, 1) ) == SEM_FAILED ) ||
		( (semaphore_mutex = sem_open("/xadamc07.mutex", O_CREAT | O_EXCL, 0666, 1) ) == SEM_FAILED )
		){
		fprintf(stderr, "Nepodarilo sa vytvorit semafor(y)!\n");
		clean_exit(1);
	}
}


void life_of_immigrant(int my_id) {

	writeToFile("IMM", "starts", my_id);
	do{
		sem_wait(semaphore_entry);
		sem_wait(semaphore_mutex);
		if(*JIDH == 1){
			sem_post(semaphore_mutex);
		}
	}while(*JIDH==1);

	*NE=*NE+1;
	*NB=*NB+1;
	writeToFile("IMM", "enters", my_id);
	sem_post(semaphore_mutex);

	if (*JIDH == 0){
		sem_post(semaphore_entry);
	}

	sem_wait(semaphore_mutex);
	*NC=*NC+1;
	writeToFile("IMM", "checks", my_id);
	sem_post(semaphore_mutex);


	if(*NE == *NC && *JIDH == 1){
		sem_post(semaphore_registration);
	}
	


	sem_wait(semaphore_judgement);
	sem_wait(semaphore_mutex);

	writeToFile("IMM", "wants certificate", my_id);
	usleep(get_random_number(*IT*1000));
	writeToFile("IMM", "got certificate", my_id);
	sem_post(semaphore_mutex);


	do{
		sem_wait(semaphore_exit);
		sem_wait(semaphore_mutex);
		if(*JIDH==1){
			sem_post(semaphore_mutex);
		}
	}while(*JIDH==1);

	*NB=*NB-1;
	writeToFile("IMM", "leaves", my_id);
	if(*JIDH == 0 && *NB != 0){
		sem_post(semaphore_exit);
	}
	if(*PI == 0 && *NB == 0){
		sem_post(semaphore_imm_gen);
	}
	sem_post(semaphore_mutex);
	exit(0);
}


void generate_immigrants() {
	int private_id = 1;
	int n_of_imms = *PI;
	while (n_of_imms > 0) {
		usleep(get_random_number(*IG*1000));
		pid_t id = fork();
		if (id == 0) {
			life_of_immigrant(private_id);
		}
		if (id < 0) {
			clean_exit(1);
		}
		private_id++;
		n_of_imms = n_of_imms-1;
	}
	*PI=0;
	sem_wait(semaphore_imm_gen);
}

void life_of_judge(int todays_work) {
	int temp = 0;
	do {
		usleep(get_random_number(*JG*1000));
		
		sem_wait(semaphore_mutex);
		*JIDH=1;
		sem_trywait(semaphore_entry);
		//sem_trywait(semaphore_judgement);

		for(int i = 0; i <= *NB; i++){
			sem_trywait(semaphore_exit);
		}
		writeToFile("JUDGE","wants to enter", 0);
		writeToFile("JUDGE","enters", 0);


		if (*NE != *NC){
			writeToFile("JUDGE","waits for imm", 0);
			sem_post(semaphore_mutex);

			sem_wait(semaphore_registration);
			sem_wait(semaphore_mutex);
		} 

		writeToFile("JUDGE","starts confirmation", 0);
		sem_post(semaphore_mutex);
		usleep(get_random_number(*JT*1000));

		sem_wait(semaphore_mutex);
		todays_work = todays_work - *NC;
		temp=*NC;
		*NE=0;
		*NC=0;
		writeToFile("JUDGE","ends confirmation", 0);
		for (int i = 0; i < temp; ++i)
		{
			sem_post(semaphore_judgement);
		}
		
		sem_post(semaphore_mutex);

		usleep(get_random_number(*JT*1000));
		sem_wait(semaphore_mutex);
		writeToFile("JUDGE", "leaves", 0);
		*JIDH=0;

		sem_trywait(semaphore_registration);
		
		for (int i = 0; i <= *NB; ++i){
			sem_post(semaphore_exit);
		}

		sem_post(semaphore_entry);
		sem_post(semaphore_mutex);
		
		
	} while(todays_work>0);

	writeToFile("JUDGE", "finishes", 0);
}



int main(int argc, char **argv) {

	init();

	char *p;
	if (argc != 6){
		fprintf(stderr, "Chyba v pocte argumentov!\n");
		clean_exit(1);
	}

	*PI=(int)strtol(argv[1], &p, 10);
	*IG=(int)strtol(argv[2], &p, 10);
	*JG=(int)strtol(argv[3], &p, 10);
	*IT=(int)strtol(argv[4], &p, 10);
	*JT=(int)strtol(argv[5], &p, 10);
	if(errno != 0 || 
		*PI < 0 || *PI > 2000 ||
		*IG < 0 || *IG > 2000 ||
		*JG < 0 || *JG > 2000 ||
		*IT < 0 || *IT > 2000 ||
		*JT < 0 || *JT > 2000){
		fprintf(stderr, "Chyba v hodnotach argumentov!\n");
		clean_exit(1);
	}

	pid_t id_of_process;


	id_of_process = fork();
	if (id_of_process == 0) {
		generate_immigrants();
		sem_post(semaphore_main);
		exit(0);
	}
	if (id_of_process < 0) {
		clean_exit(1);
	}


	id_of_process = fork();
	if (id_of_process == 0) {
		life_of_judge(*PI);
		sem_post(semaphore_main);
		exit(0);
	}
	if (id_of_process < 0) {
		clean_exit(1);
	}

	sem_wait(semaphore_main);
	sem_wait(semaphore_main);

	clean_exit(0);
}
