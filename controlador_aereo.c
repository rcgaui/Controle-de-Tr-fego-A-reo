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
#define NUM_AERONAVES 3

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
    int em_proximidade;
    int ultimo_ciclo_reducao;
} Aeronave;

Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso, double coordenada_x, double velocidade, double distancia_pouso, int velocidade_reduzida, int pista_alternativa, int pouso_realizado, int em_proximidade);
int pista(int lado_entrada, double coordenada_y);
double distancia(double coordenada_x, double coordenada_y);
int proximidade(Aeronave *mem);
int pista_livre(Aeronave *mem, int pista);
int calcula_pista_alternativa(int lado_entrada, int pista_atual);
void toggle_velocidade(int sig);  
void toggle_pista_alternativa(int sig); 
void atualizar_aeronave_local(Aeronave *local, Aeronave *mem);
int global_shmid; // novo: permite acesso à memória no handler


int main() {
    int shmid = shmget(IPC_PRIVATE, sizeof(Aeronave) * NUM_AERONAVES, IPC_CREAT | 0666);
    if (shmid < 0) {
        perror("shmget");
        exit(1);
    }
    global_shmid = shmid;

    Aeronave *mem = (Aeronave *) shmat(shmid, NULL, 0);
    if (mem == (Aeronave *) -1) {
        perror("shmat");
        exit(1);
    }

    for (int i = 0; i < NUM_AERONAVES; i++) {
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
            int pista_alternativa = calcula_pista_alternativa(lado_entrada, pista_pouso);
            int pouso_realizado = 0;
            int em_proximidade = 0;
            pid_t meu_pid = getpid();
            const char *lado = lado_entrada == 0 ? "Oeste" : "Leste";

            printf("[Aeronave %d | PID %d] Atraso: %ds\n",i + 1, meu_pid, atraso);
            sleep(atraso);
            Aeronave x = cria_aeronave(meu_pid, lado_entrada, coordenada_y, atraso, pista_pouso, coordenada_x, velocidade, distancia_pouso, velocidade_reduzida, pista_alternativa, pouso_realizado, em_proximidade);
            printf("[Aeronave %d | PID %d] Entrou no espaço aéreo.\n", i + 1, meu_pid);
            mem[i] = x;
            signal(SIGUSR1, toggle_velocidade);//handler 1
            signal(SIGUSR2, toggle_pista_alternativa);//handler 2

            while (x.distancia_pouso > 0.01) {
                sleep(1);
                atualizar_aeronave_local(&x, &mem[i]);
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
    int ciclo_atual = 0;
    while (!todas_pousaram) {
        sleep(1);
        ciclo_atual++;
        printf("\n--- Status das Aeronaves ---\n");
        todas_pousaram = 1;
        for (int i = 0; i < NUM_AERONAVES; i++) {
            printf("Aeronave %d: | Distância: %.2f | Coordenadas: %.2f, %.2f | Pouso: %s | Pista: %d | Velocidade: %.2f\n",
                   i + 1, mem[i].distancia_pouso, mem[i].coordenada_x, mem[i].coordenada_y,
                   mem[i].pouso_realizado ? "Sim" : "Não", mem[i].pista_pouso, mem[i].velocidade);
            if (!mem[i].pouso_realizado) {
                todas_pousaram = 0;
            }
            // Só normaliza se não estiver em proximidade e não foi reduzida neste ciclo
            if (mem[i].velocidade_reduzida == 1 && mem[i].em_proximidade == 0 && mem[i].ultimo_ciclo_reducao != ciclo_atual) {
                kill(mem[i].pid, SIGUSR1);
            }
        }
        int colisao = proximidade(mem);
        printf("---------------------------\n");
    }

    for (int i = 0; i < NUM_AERONAVES; i++) {
        wait(NULL);
    }

    for (int i = 0; i < NUM_AERONAVES; i++) {
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

Aeronave cria_aeronave(pid_t pid, char lado_entrada, double coordenada_y, int atraso, int pista_pouso, double coordenada_x, double velocidade, double distancia_pouso, int velocidade_reduzida, int pista_alternativa, int pouso_realizado, int em_proximidade) {
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
    aviao.em_proximidade = em_proximidade;
    aviao.ultimo_ciclo_reducao = 0;
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

int proximidade(Aeronave *mem) {
    static int ciclo_atual = 0;
    ciclo_atual++;
    for (int i = 0; i < NUM_AERONAVES; i++) {
        mem[i].em_proximidade = 0;
    }
    for (int i = 0; i < NUM_AERONAVES; i++) {
        if (mem[i].pouso_realizado == 0 && mem[i].distancia_pouso != 0.0) {
            for (int j = i+1; j < NUM_AERONAVES; j++) {
                if (mem[j].pouso_realizado == 0 && mem[j].distancia_pouso != 0.0) {
                    double distancia_x_entre_aeronaves = fabs(mem[i].coordenada_x - mem[j].coordenada_x);
                    double distancia_y_entre_aeronaves = fabs(mem[i].coordenada_y - mem[j].coordenada_y);

                    if (distancia_x_entre_aeronaves < 0.1 && distancia_y_entre_aeronaves < 0.1 && mem[i].pista_pouso == mem[j].pista_pouso) {
                        printf("Possível colisão entre [Aeronave %d | PID %d | Pista: %d] e [Aeronave %d | PID %d | Pista: %d]\n",
                                 i+1, mem[i].pid, mem[i].pista_pouso, j+1, mem[j].pid, mem[j].pista_pouso);

                        mem[i].em_proximidade = 1;
                        mem[j].em_proximidade = 1;

                        int pista_alt = calcula_pista_alternativa(mem[i].lado_entrada, mem[i].pista_pouso);
                        int pista_alt_livre = pista_livre(mem, pista_alt);
                        int aeronave_mais_distante = (mem[i].distancia_pouso > mem[j].distancia_pouso) ? i : j; 

                        if (pista_alt_livre) {
                            mem[aeronave_mais_distante].pista_alternativa = pista_alt;
                            printf("Sugestão de troca de pista: [Aeronave %d | PID %d | Pista alternativa: %d]\n", aeronave_mais_distante+1, mem[aeronave_mais_distante].pid, pista_alt);
                            kill(mem[aeronave_mais_distante].pid, SIGUSR2);
                        }
                        else {
                            printf("Sugestão de redução de velocidade: [Aeronave %d | PID %d | Pista: %d]\n", aeronave_mais_distante+1, mem[aeronave_mais_distante].pid, mem[aeronave_mais_distante].pista_pouso);
                            if (!mem[aeronave_mais_distante].velocidade_reduzida) {
                                mem[aeronave_mais_distante].ultimo_ciclo_reducao = ciclo_atual;
                                kill(mem[aeronave_mais_distante].pid, SIGUSR1);
                            }
                        }
                    }
                }
            }
        }
    }
    return -1;
}

int calcula_pista_alternativa(int lado_entrada, int pista_atual) {
    if (lado_entrada == 0)
        return pista_atual == 3 ? 18 : 3;
    else
        return pista_atual == 6 ? 27 : 6;
}

int pista_livre(Aeronave *mem, int pista) {
    for (int i = 0; i < NUM_AERONAVES; i++) {
        if (mem[i].pouso_realizado == 0 && mem[i].pista_pouso == pista) {
            return 0;
        }
    }
    return 1;
}

//handler 1
void toggle_velocidade(int sig) {
    Aeronave *mem = (Aeronave *) shmat(global_shmid, NULL, 0);
    if (mem == (Aeronave *) -1) {
        perror("shmat no sinal");
        exit(1);
    }
    for (int i = 0; i < 5; i++) {
        if (mem[i].pid == getpid()) {
            if (mem[i].velocidade == 0.0) {
                // Só normaliza se não estiver em proximidade
                if (!mem[i].em_proximidade) {
                    mem[i].velocidade = VELOCIDADE;
                    mem[i].velocidade_reduzida = 0;
                    printf("[Aeronave %d | PID %d] Velocidade NORMALIZADA por sinal\n", i+1, getpid());
                }
            } else {
                mem[i].velocidade = 0.0;
                mem[i].velocidade_reduzida = 1;
                printf("[Aeronave %d | PID %d] Velocidade REDUZIDA por sinal\n", i+1, getpid());
            }
            break;
        }
    }
    shmdt(mem);
}//fim do handler 1

void toggle_pista_alternativa(int sig) {
    Aeronave *mem = (Aeronave *) shmat(global_shmid, NULL, 0);
    if (mem == (Aeronave *) -1) {
        perror("shmat no sinal");
        exit(1);
    }
    for (int i = 0; i < NUM_AERONAVES; i++) {
        if (mem[i].pid == getpid()) {
            int pista_antiga = mem[i].pista_pouso;
            mem[i].pista_pouso = mem[i].pista_alternativa;
            mem[i].pista_alternativa = calcula_pista_alternativa(mem[i].lado_entrada, mem[i].pista_pouso);
            printf("[Aeronave %d | PID %d] Pista alterada de %d para %d\n", i+1, getpid(), pista_antiga, mem[i].pista_pouso);
            break;
        }
    }
    shmdt(mem);
}

void atualizar_aeronave_local(Aeronave *local, Aeronave *mem) {
    local->velocidade = mem->velocidade;
    local->velocidade_reduzida = mem->velocidade_reduzida;
    local->pista_pouso = mem->pista_pouso;
    local->pista_alternativa = mem->pista_alternativa;
}