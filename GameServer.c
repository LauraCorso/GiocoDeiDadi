#include<stdio.h>
#include<stdlib.h>
#include<time.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/sem.h>
#include<sys/types.h>
#include<unistd.h>
#include<sys/wait.h>
#include<signal.h>

#define KEY1 15
#define KEY2 20
#define KEY3 25

//------------------------------ Variabili globali -------------------------------------------------------------
// Strutture per i semafori
 struct sembuf sleepS = {.sem_op = -1, .sem_flg = 0}; //decrementa il semaforo del server
 struct sembuf awakeG = {.sem_op = 1, .sem_flg = 0}; //aumenta il semaforo di un giocatore
 
// Struttura per la stampa finale delle partite giocate/vinte/perse
typedef struct {
	int giocate;
	int perse;
	int vinte;
} datiP;
datiP partite = {0, 0, 0};

int pidF = -1; //PID del figlio
int sommaI; //somma iniziale
int somma; //soldi del banco (in aggiornamento)


//------------------------------ Funzioni ------------------------------------------------------------------------
void termina(int sig) {
	exit(1);
}

void stampa() {
	printf("\n\n");
	printf("-------------------------------------------------\n");
	printf("|                 RIASSUNTO                      \n");
	printf("|------------------------------------------------\n");
	printf("|   partite giocate   |            %i            \n", partite.giocate);
	printf("|------------------------------------------------\n");
	printf("|    partite vinte    |            %i            \n", partite.vinte);
	printf("|------------------------------------------------\n");
	printf("|    partite perse    |            %i            \n", partite.perse);
	printf("|------------------------------------------------\n");
	printf("|   monete iniziali   |            %i            \n", sommaI);
	printf("|------------------------------------------------\n");
	printf("|    monete finali    |            %i            \n", somma);
	printf("-------------------------------------------------\n");
	printf("\n\n");
}

void giocatoreTerminato(int sig) {
	printf("\n\n");
	printf("Un giocatore o piu' ha/hanno lasciato il gioco\n");
}

void termino(int sig) {
	int stato; //ritorno del figlio alla terminazione
	int mS, *vS; //id e puntatore della memoria condivisa 1
	int mL, *vL; //id e puntatore della memoria condivisa 2
	int mP; //id della memoria condivisa 3
	int j; //contatore per i cicli
	int p_mem1; //posizione dei giocatori nel semaforo 1
	
	// Accedo alle aree di memoria condivisa
	 mS = shmget(KEY1, 0, 0644);
	 vS = shmat(mS, NULL, 0);
	 mL = shmget(KEY2, 0, 0644);
	 vL = shmat(mL, NULL, 0);
	 mP = shmget(KEY3, 0, 0644);
	 
	system("clear");
	// Comunico l'interruzione del server ai giocatori
	 if (sig == 12) {
	 	printf("Banco: bancarotta\n");
	 	vS[1] = 0;
	 }
	 printf("Chiusura gioco in corso...\n\n");
	 for (j = 1; j < vL[1]+1; j++) {
	 	p_mem1 = j+1;
	 	if (vS[p_mem1] != 0) {
	 		awakeG.sem_num = j;
			semop(vS[0], &awakeG, 1);
	 	}
	 } 
	 
	// Termino il figlio
	 kill(pidF, 12);
	 wait(&stato);
	 
	// Elimino i semafori
	 semctl(vS[0], 0, IPC_RMID);
	 semctl(vL[0], 0, IPC_RMID);
	
	// Elimino l'area di memoria condivisa
	 shmctl(mS, IPC_RMID, NULL);
	 shmctl(mL, IPC_RMID, NULL);
	 shmctl(mP, IPC_RMID, NULL);
	 
	printf("GIOCO TERMINATO\n\n");
	stampa();
	exit(1);
}

void ctrlc (int sig) {
	char r;
	printf("   Hai premuto CTRL+C. Vuoi terminare? (y/n) ");
	scanf(" %c", &r);
	if (r == 'y') {
		termino(2);
	}
	else {
		SIG_IGN;
	}
}

void attesa(int *mem2, int n) {
	int numG = semctl(mem2[0], 0, GETVAL);
	printf("Numero di giocatori: %i su %i\n", numG, mem2[1]);
	printf("In attesa di %i giocatori\n", mem2[1]-numG);
	printf("\n\n");
	while (numG < n) {
		// Addormento il server
		 sleepS.sem_num = 1;
		 semop(mem2[0], &sleepS, 1);
		 
		// Al risveglio
		 numG = semctl(mem2[0], 0, GETVAL);
		 printf("Numero di giocatori: %i su %i\n", numG, mem2[1]);
		 printf("In attesa di %i giocatori\n", mem2[1]-numG);
		 printf("Attualmente online ci sono %i giocatori\n", numG);
		 printf("\n\n");
	}
	
	printf("Tutti i %i giocatori sono online. INIZIAMO \n\n", n);
}

void scommesse(int *l) {
	int i;
	for (i = 2; i < l[1]+2; i++) {
		// Risveglio il giocatore i-esimo
		 awakeG.sem_num = i;
		 semop(l[0], &awakeG, 1);
		 
		// Mi addormento
		 sleepS.sem_num = 1;
		 semop(l[0], &sleepS, 1);
		 
		// Mi risveglio
		 sleep(1);
	}
}

void lancio(int *l) {
	int i;
	for (i = 2; i < l[1]+2; i++) {
		// Risveglio il giocatore i-esimo
		 awakeG.sem_num = i;
		 semop(l[0], &awakeG, 1);
		 
		// Mi addormento
		 sleepS.sem_num = 1;
		 semop(l[0], &sleepS, 1);
		 
		// Mi risveglio
		 sleep(1);
	}
}

int contenuto(int *a, int n, int val) {
	int *fineA = a+n;
	for ( ; a < fineA; ++a) {
		if (*a == val) {
			return 1;
		}
	}
	
	return 0;
}

//------------------------------ Main -----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
	// Dichiarazione delle variabili
	 int n; //numero di giocatori
	 int smS_ID, *status; //id e puntatore della memoria condivisa 1
	 int smL_ID, *lanci; //id e puntatore della memoria condivisa 2
	 int smP_ID, *puntate; //id e puntatore della memoria condivisa 3
	 int semS_ID; //id semaforo 1
	 int semL_ID; //id semaforo 2
	 int i, j; //contatori
	 int max; //punteggio dadi maggiore
	 
	
	// Controllo i parametri inseriti da riga di comando
	 if (argc != 3) {
	 	printf("\nErrore!\nUso: \"./GameServer <numeroGiocatori> <euroBanco>\" \n\n");
		exit(1);
	 }
	 else {
	 	n = atoi(argv[1]);
	 	sommaI = atoi(argv[2]);
	 	if (n < 1 && sommaI <= 0) {
	 		printf("\nErrore!\nPer giocare serve almeno 1 giocatore \n\n");
	 		printf("\nErrore!\nIl banco deve avere più di 0 euro \n\n");
			exit(1);
	 	}
	 	else if (n < 1) {
	 		printf("\nErrore!\nPer giocare serve almeno 1 giocatore \n\n");
			exit(1);
	 	}
	 	else if (sommaI <= 0) {
	 		printf("\nErrore!\nIl banco deve avere più di 0 euro \n\n");
			exit(1);
	 	}
	 }
	
	system("clear");
	int winners[n+1]; //vincitore/i
	for (i = 0; i < n+1; i++) {
		winners[i] = 0;
	} 
	 
	/* Area di memoria 1. Contiene un vettore dove
	   [0] = id semaforo 1
	   [1] = server termina per bancarotta(0)/per CTRL+C
	   [2 ... n+2] = giocatore attivo(1)/interrotto(0) */
	 smS_ID = shmget(KEY1, sizeof(int)*(n+2), IPC_CREAT | 0644);
	 status = shmat(smS_ID, NULL, 0);
	 status[1] = 3;
	 for (i = 2; i < n+2; i++) {
	 	status[i] = 0;
	 }
	
	/* Area di memoria 2. Contiene un vettore dove
	   [0] = id semaforo 2
	   [1] = numero di giocatori inserito da tastiera
	   [2] = risultato lancio dadi del banco
	   [3 ... n+3] = risultato lancio dadi dei giocatori */
	 smL_ID = shmget(KEY2, sizeof(int)*(n+3), IPC_CREAT | 0644);
	 lanci = shmat(smL_ID, NULL, 0);
	 lanci[1] = n;
	 
	// Area di memoria 3. Contiene un vettore dove ogni cella contiene la cifra puntata da ogni giocatore 
	 smP_ID = shmget(KEY3, sizeof(int)*n, IPC_CREAT | 0644);
	 puntate = shmat(smP_ID, NULL, 0);
	 
	/* Semaforo 1. Vettore dove
	   [0] = Sveglia/addormenta server 
	   [1 ... n+1] = sveglia/addormenta giocatori */
	 semS_ID = semget(KEY1, n+1, IPC_CREAT | 0644);
	 status[0] = semS_ID;
	
	/* Semaforo 2. Vettore dove
	   [0] = numero giocatori --> cresce/decresce al giocare dei giocatori
	   [1] = sveglia/addormenta server
	   [2 ... n+2] = sveglia/addormenta giocatore corrispondente */
	 semL_ID = semget(KEY2, n+2, IPC_CREAT | 0644);
	 lanci[0] = semL_ID;
	 

	// Creazione figlio per il controllo terminazione giocatori
	 pidF = fork();
	 if (pidF == 0) {
		signal(12, termina);
		signal(2, SIG_IGN);
		while (1) {
			sleepS.sem_num = 0;
			semop(semS_ID, &sleepS, 1); //sospendo
			
			// Al risveglio
			 kill(getppid(), 10);
		}
	 }
	 else {
		// Il padre controlla il gioco
		signal(10, giocatoreTerminato);
		signal(2, ctrlc);
		signal(12, termino);
		somma = sommaI;
		
		// Gioco
		while(1) {
			if (somma > 0) {
				// Collegamento dei giocatori fino al numero prestabilito
				 if (partite.giocate == 0 || semctl(semL_ID, 0, GETVAL) < n) {
					if (semctl(semL_ID, 0, GETVAL) < n && partite.giocate != 0) {
						for (i = 2; i < n+2; i++) {
							if (status[i] == 0) {
								printf("Il giocatore %i ha abbandonato il gioco\n\n", i-1);
							}
						}
					}
					attesa(lanci, n);
				 }
				
				printf("\n\n");
				printf("PARTITA N. %i\n", partite.giocate+1);
								
				// I giocatori puntano i soldi
				 scommesse(lanci);
				 
				// Il banco lancia i dadi
				 printf("Lancio dei dadi in corso...\n");
				 srand(time(NULL));
				 lanci[2] = rand() %11 +2;
				 printf("Hai ottenuto: %i\n", lanci[2]);
				 sleep(1);
				 
				// I giocatori lanciano i dadi
				 lancio(lanci);
				 
				// Determino il punteggio massimo
				 max = 0;
				 for (i = 2; i <= lanci[1] +2; i++) {
				 	if (max == 0 || lanci[i] > max) {
				 		max = lanci[i];
				 	}
				 }
				 
				// Determino il/i vincitore/i
				 j = 0;
				 for (i = 2; i <= lanci[1] +2; i++) {
				 	if (lanci[i] == max) {
				 		winners[j] = i;
				 		j++;
				 	}
				 }
				 
				// Regolo i conti
				 if (winners[0] == 2) {
				 	if (j == 1) {
				 		// Ha vinto il server
				 		printf("\n\n");
				 		printf("HAI VINTO\n\n");
				 		
				 		for (i = 0; i < n; i++) {
				 			somma += puntate[i];
				 		}
				 		
				 		partite.vinte++;
				 		
				 		// Sveglio i giocatori per comunicare
				 		 lanci[2] = 0;
				 		 for(i = 2; i < n+2; i++) {
				 		 	awakeG.sem_num = i;
				 		 	semop(semL_ID, &awakeG, 1);
				 		 	
				 		 	sleepS.sem_num = 1;
				 		 	semop(semL_ID, &sleepS, 1);
				 		 }
				 	}
				 	else {
				 		// Il server ha pareggiato con uno o più giocatori
				 		 for(i = 3; i < n+3; i++) {
				 		 	if (contenuto(winners, j, i)) {
				 		 		lanci[2] = -1;
				 		 		printf("\n\n");
				 		 		printf("Ha vinto il giocatore %i\n", i-2);
				 		 		somma -= (puntate[i-3]*2);
				 			
					 			// Sveglio il giocatore corrispondente
					 			 awakeG.sem_num = i-1;
					 			 semop(semL_ID, &awakeG, 1);
					 			 
					 			// Addormento il server
					 			 sleepS.sem_num = 1;
					 			 semop(semL_ID, &sleepS, 1);
				 		 	}
				 			else {
				 				lanci[2] = -2;
				 				somma += puntate[i-3];
				 				
				 				// Sveglio il giocatore corrispondente
					 			 awakeG.sem_num = i-1;
					 			 semop(semL_ID, &awakeG, 1);
					 			 
					 			// Addormento il server
					 			 sleepS.sem_num = 1;
					 			 semop(semL_ID, &sleepS, 1);
				 			}
				 		}
				 		
				 		partite.perse++;
				 	}
				 }
				 else {
				 	// Uno o più giocatori hanno vinto
				 	 for(i = 3; i < n+3; i++) {
				 		 	if (contenuto(winners, j, i)) {
				 		 		lanci[2] = -1;
				 		 		printf("\n\n");
				 		 		printf("Ha vinto il giocatore %i\n", i-2);
				 		 		somma -= (puntate[i-3]*2);
				 			
					 			// Sveglio il giocatore corrispondente
					 			 awakeG.sem_num = i-1;
					 			 semop(semL_ID, &awakeG, 1);
					 			 
					 			// Addormento il server
					 			 sleepS.sem_num = 1;
					 			 semop(semL_ID, &sleepS, 1);
				 		 	}
				 			else {
				 				lanci[2] = -2;
				 				somma += puntate[i-3];
				 				
				 				// Sveglio il giocatore corrispondente
					 			 awakeG.sem_num = i-1;
					 			 semop(semL_ID, &awakeG, 1);
					 			 
					 			// Addormento il server
					 			 sleepS.sem_num = 1;
					 			 semop(semL_ID, &sleepS, 1);
				 			}
				 		}
				 		
				 	partite.perse++;
				 }
				partite.giocate++;
				if (somma <= 0) {
					kill(getpid(), 12);
				}
			}
			printf("\n\n");
			printf("In attesa di iniziare una nuova partita...\n");
			sleep(10);
		
		}
	 }
	return 0;
}


/*
 Matricola: VR429604
 Nome Cognome: LAURA CORSO
 Data di realizzazione: 27/05/2020
 Titolo: Secondo elaborato - UNIX System Call ~ GameServer
*/
