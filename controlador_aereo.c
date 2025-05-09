#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>


typedef struct {
    pid_t pid;
    int lado_entrada; // 0 oeste 1 leste
    double coordenada_y;
    int atraso;
    int pista_pouso;
} Aeronave;


Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso);

int pista(int lado_entrada, double coordenada_y);


int main() {
    int num_aeronave = 5;

    int shmid = shmget(IPC_PRIVATE, sizeof(Aeronave) * num_aeronave, IPC_CREAT | 0666);

    if (shmid<0) {
        perror("shmget");
        exit(1);
    }

    Aeronave *mem = (Aeronave *) shmat(shmid, NULL, 0);
    
    if (mem == (Aeronave *) -1) {
        perror("shmat");
        exit(1);
    }

    for(int i=0; i<num_aeronave; i++){
        pid_t pid = fork();

        if (pid == 0) {
            srand(time(NULL) + getpid());
            
            int lado_entrada = rand()%2;
            double coordenada_y = (double) rand() / RAND_MAX;
            int atraso = rand()%3;
            int pista_pouso = pista(lado_entrada, coordenada_y);

            Aeronave x = cria_aeronave(getpid(), lado_entrada, coordenada_y, atraso, pista_pouso);
            
            mem[i] = x;

            shmdt(mem);
            exit(0);
        }
    }

    for (int i = 0; i < num_aeronave; i++) {
        wait(NULL);
    }

    for (int i = 0; i < num_aeronave; i++) {
        printf("\nAeronave %d:\n", i+1);
        printf("PID: %d\n", mem[i].pid);
        printf("Lado de entrada: %s\n", mem[i].lado_entrada == 0 ? "Oeste" : "Leste");
        printf("Coordenada Y: %.2f\n", mem[i].coordenada_y);
        printf("Atraso: %d\n", mem[i].atraso);
        printf("Pista de pouso: %d\n", mem[i].pista_pouso);
        printf("------------------------\n");
    }

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}


Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso){
    Aeronave aviao;
    
    aviao.pid = pid;
    aviao.lado_entrada = lado_entrada;
    aviao.coordenada_y = coordenada_y;
    aviao.atraso = atraso;
    aviao.pista_pouso = pista_pouso;
    
    return aviao;
}


int pista(int lado_entrada, double coordenada_y){
    if (lado_entrada == 0){ // oeste (pista 03 ou 18)
        if(coordenada_y>0.5){
            return 3;
        }
        else{
            return 18;
        }
    }
    else{ // leste (pista 06 ou 27)
        if(coordenada_y>0.5){
            return 6;
        }
        else{
            return 27;
        }
    }
}