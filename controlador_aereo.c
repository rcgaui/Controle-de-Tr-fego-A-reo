#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>

typedef struct {
    pid_t pid;
    int lado_entrada; // 0 oeste 1 leste
    double coordenada_y;
    int atraso;
    int pista_pouso;
    double coordenada_x;
    double velocidade; // velocidade padrão de 0.05/segundo
    int velocidade_reduzida; // por default nenhum avião começa com a velocidade reduzida(0 velocidade normal 1 velocidade reduzida)
    double distancia_pouso; // por default distancia de pouso 0
    int pista_alternativa; // por default nenhum avião começa com a pista alternativa(0 pista original 1 pista alternativa)
    int pouso_realizado; // por default nenhum avião começa com o pouso realizado (0 pouso não realizado 1 pouso realizado)

} Aeronave;


Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso, double coordenada_x, double velocidade, double distancia_pouso, int velocidade_reduzida, int pista_alternativa, int pouso_realizado);

int pista(int lado_entrada, double coordenada_y);
double distancia(double coordenada_x, double coordenada_y);

int main() {
    int num_aeronave = 2;

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
            // inicia variáveis das aeronaves
            srand(time(NULL) + getpid());
            int lado_entrada = rand()%2;
            double coordenada_y = (double) rand() / RAND_MAX;
            int atraso = rand()%3;
            int pista_pouso = pista(lado_entrada, coordenada_y);
            double coordenada_x = 0;
            double velocidade = 0.05;
            double distancia_pouso = distancia(coordenada_x, coordenada_y);
            int velocidade_reduzida = 0;
            int pista_alternativa = 0;
            int pouso_realizado = 0;    

            // cria aeronave
            Aeronave x = cria_aeronave(getpid(), lado_entrada, coordenada_y, atraso, pista_pouso, coordenada_x, velocidade, distancia_pouso, velocidade_reduzida, pista_alternativa, pouso_realizado);
            
            // simula pouso
            while (x.distancia_pouso > 0) {
                sleep(1);
                x.distancia_pouso -= 0.05;

                mem[i] = x;

                if (x.distancia_pouso <= 0) {
                    x.pouso_realizado = 1;
                    mem[i] = x;
                }
            }

            shmdt(mem);
            exit(0);
        }
    }

    int todas_pousaram = 0;
    while (!todas_pousaram) {
        sleep(1);

        printf("\n--- Status das Aeronaves ---\n");
        todas_pousaram = 1;

        for (int i = 0; i < num_aeronave; i++) {
            printf("Aeronave %d: | Distância aeroporto: %.2f | Pouso realizado: %s\n",
                   i+1, mem[i].distancia_pouso, mem[i].pouso_realizado == 1 ? "Sim" : "Não");

            if (mem[i].pouso_realizado == 0) {
                todas_pousaram = 0;
            }
        }
        printf("---------------------------\n");
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
        printf("Coordenada X: %.2f\n", mem[i].coordenada_x);
        printf("Velocidade: %.2f\n", mem[i].velocidade);
        printf("Distancia de pouso: %.2f\n", mem[i].distancia_pouso);
        printf("Velocidade reduzida: %d\n", mem[i].velocidade_reduzida);
        printf("Pista alternativa: %d\n", mem[i].pista_alternativa);
        printf("Pouso realizado: %d\n", mem[i].pouso_realizado);
        printf("------------------------\n");
    }

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);

    return 0;
}

Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso, double coordenada_x, double velocidade, double distancia_pouso, int velocidade_reduzida, int pista_alternativa, int pouso_realizado){
    Aeronave aviao;
    
    aviao.pid = pid;
    aviao.lado_entrada = lado_entrada;
    aviao.coordenada_y = coordenada_y;
    aviao.atraso = atraso;
    aviao.pista_pouso = pista_pouso;
    aviao.coordenada_x = coordenada_x;
    aviao.velocidade = velocidade;
    aviao.distancia_pouso = distancia_pouso;
    aviao.velocidade_reduzida = velocidade_reduzida;
    aviao.pista_alternativa = pista_alternativa;
    aviao.pouso_realizado = pouso_realizado;

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

double distancia(double coordenada_x, double coordenada_y){
    return sqrt(pow(coordenada_x - 0.5, 2) + pow(coordenada_y - 0.5, 2));
}   