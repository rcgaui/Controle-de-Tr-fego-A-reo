#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <signal.h>
#include <math.h>

#define VELOCIDADE 0.05

typedef struct {
    pid_t pid;
    int lado_entrada; // 0 oeste 1 leste
    double coordenada_y;
    double y_inicial;
    int atraso;
    int pista_pouso;
    double coordenada_x;
    double x_inicial;
    double velocidade;
    int velocidade_reduzida;
    double distancia_pouso;
    double distancia_inicial;
    int pista_alternativa;
    int pouso_realizado;
} Aeronave;

Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso, double coordenada_x, double velocidade, double distancia_pouso, int velocidade_reduzida, int pista_alternativa, int pouso_realizado);
int pista(int lado_entrada, double coordenada_y);
double distancia(double coordenada_x, double coordenada_y);

int main() {
    int num_aeronave = 5;
    int shmid = shmget(IPC_PRIVATE, sizeof(Aeronave) * num_aeronave, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }

    Aeronave *mem = (Aeronave *) shmat(shmid, NULL, 0);
    if (mem == (Aeronave *) -1) {
        perror("shmat");
        exit(1);
    }

    for (int i = 0; i < num_aeronave; i++) {
        pid_t pid = fork();
        if (pid == 0) {
            srand(time(NULL) + getpid());
            int lado_entrada = rand() % 2;
            double coordenada_y = (double) rand() / RAND_MAX;
            int atraso = rand() % 3;
            int pista_pouso = pista(lado_entrada, coordenada_y);
            double coordenada_x = (lado_entrada == 0) ? 0.0 : 1.0;
            double velocidade = VELOCIDADE;
            double distancia_pouso = distancia(coordenada_x, coordenada_y);
            int velocidade_reduzida = 0;
            int pista_alternativa = 0;
            int pouso_realizado = 0;
            pid_t meu_pid = getpid();
            const char *lado = lado_entrada == 0 ? "Oeste" : "Leste";

            printf("[Aeronave %d | PID %d] Atraso: %ds\n",i + 1, meu_pid, atraso);
            sleep(atraso);
            Aeronave x = cria_aeronave(meu_pid, lado_entrada, coordenada_y, atraso, pista_pouso, coordenada_x, velocidade, distancia_pouso, velocidade_reduzida, pista_alternativa, pouso_realizado);
            printf("[Aeronave %d | PID %d] Entrou no espaço aéreo.\n", i + 1, meu_pid);
            mem[i] = x;

            while (x.distancia_pouso > 0.01) {
                sleep(1);
                if (x.coordenada_x < 0.5) {
                    x.coordenada_x += x.velocidade;
                    if (x.coordenada_x > 0.5) x.coordenada_x = 0.5;
                } else if (x.coordenada_x > 0.5) {
                    x.coordenada_x -= x.velocidade;
                    if (x.coordenada_x < 0.5) x.coordenada_x = 0.5;
                }

                if (x.coordenada_y < 0.5) {
                    x.coordenada_y += x.velocidade;
                    if (x.coordenada_y > 0.5) x.coordenada_y = 0.5;
                } else if (x.coordenada_y > 0.5) {
                    x.coordenada_y -= x.velocidade;
                    if (x.coordenada_y < 0.5) x.coordenada_y = 0.5;
                }

                x.distancia_pouso = distancia(x.coordenada_x, x.coordenada_y);
                mem[i] = x;
            }

            x.pouso_realizado = 1;
            mem[i] = x;

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
            printf("Aeronave %d: | Distância: %.2f | Coordenadas: %.2f, %.2f | Pouso: %s\n",
                   i + 1, mem[i].distancia_pouso, mem[i].coordenada_x, mem[i].coordenada_y,
                   mem[i].pouso_realizado ? "Sim" : "Não");
            if (!mem[i].pouso_realizado) {
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
        printf("Coordenada Y: %.2f\n", mem[i].y_inicial);
        printf("Coordenada X: %.2f\n", mem[i].x_inicial);
        printf("Atraso: %ds\n", mem[i].atraso);
        printf("Pista de pouso: %d\n", mem[i].pista_pouso);
        printf("Velocidade: %.2f\n", mem[i].velocidade);
        printf("Distancia de pouso inicial: %.2f\n", mem[i].distancia_inicial);
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

Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso, double coordenada_x, double velocidade, double distancia_pouso, int velocidade_reduzida, int pista_alternativa, int pouso_realizado) {
    Aeronave aviao;
    aviao.pid = pid;
    aviao.lado_entrada = lado_entrada;
    aviao.coordenada_y = coordenada_y;
    aviao.y_inicial = coordenada_y;
    aviao.atraso = atraso;
    aviao.pista_pouso = pista_pouso;
    aviao.coordenada_x = coordenada_x;
    aviao.x_inicial = coordenada_x;
    aviao.velocidade = velocidade;
    aviao.distancia_pouso = distancia_pouso;
    aviao.distancia_inicial = distancia_pouso;
    aviao.velocidade_reduzida = velocidade_reduzida;
    aviao.pista_alternativa = pista_alternativa;
    aviao.pouso_realizado = pouso_realizado;
    return aviao;
}

int pista(int lado_entrada, double coordenada_y) {
    if (lado_entrada == 0) {
        return coordenada_y > 0.5 ? 3 : 18;
    } else {
        return coordenada_y > 0.5 ? 6 : 27;
    }
}

double distancia(double coordenada_x, double coordenada_y) {
    return sqrt(pow(coordenada_x - 0.5, 2) + pow(coordenada_y - 0.5, 2));
}