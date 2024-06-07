#include<stdio.h>
#include<sys/ipc.h>
#include<sys/shm.h>
#include<sys/types.h>
#include<sys/sem.h>
#include<stdlib.h>
#include<signal.h>
#include<unistd.h>
#include<sys/wait.h>
#include<time.h>

#define KEY1 15
#define KEY2 20
#define KEY3 25

//------------------------------ Variabili globali -------------------------------------------------------------
// Strutture per i semafori
struct sembuf piu = {.sem_op = 1, .sem_flg = 0};
struct sembuf meno = {.sem_op = -1, .sem_flg = 0};

int n_giocatore; //numero del giocatore
int pidF = -1; //PID del figlio
int sommaI; //somma iniziale
int somma; //somma in cambiamento

// Struttura per la stampa finale delle partite giocate/vinte/perse
typedef struct {
	int giocate;
	int perse;
	int vinte;
} datiP;

datiP partite = {0, 0, 0};


//------------------------------ Funzioni ------------------------------------------------------------------------
void termina(int sig) {
	exit(0);
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

void server_shutdown(int sig) {
	int mS, *vS; //id e puntatore della memoria condivisa 1
	int stato; //valore di ritorno del figlio
	
	// Accedo alla memoria 1
	 mS = shmget(KEY1, 0, 0644);
	 vS = shmat(mS, NULL, 0);
	 
	system("clear");
	if (vS[1] == 0) {
		printf("\n\n");
		printf("Il banco e' andato in banca rotta\n");
		printf("Chiusura partita in corso...\n");
	}
	else {
		printf("\n\n");
		printf("Il banco e' terminato\n");
		printf("Chiusura partita in corso...\n");
	}

	// Termino il figlio
	 kill(pidF, 9);
	 wait(&stato);
	 
	 printf("\n\n");
	 printf("PARTITA TERMINATA\n\n");
	 stampa();
	 exit(1);
}

void ctrlc(int sig) {
	int mS, *vS; //id e puntatore della memoria condivisa 1
	int mL, *vL; //id e puntatore della memoria condivisa 2
	int stato; //valore di ritorno del figlio
	
	system("clear");
	if (sig == 15) {
		printf("Hai finito i soldi\n");
	}
	if (sig == 2) {
		printf("Hai premuto CTRL+C\n");
	}
	printf("Terminazione in corso...\n\n");
	
	// Accedo alle memorie
	 mS = shmget(KEY1, 0, 0644);
	 vS = shmat(mS, NULL, 0);
	 mL = shmget(KEY2, 0, 0644);
	 vL = shmat(mL, NULL, 0);
		 
	// Modifico lo stato
	 vS[n_giocatore+2] = 0;
	 
	// Modifico il numero di giocatori
	 meno.sem_num = 0;
	 semop(vL[0], &meno, 1);
	 
	// Sveglio il server
	 piu.sem_num = 0;
	 semop(vS[0], &piu, 1);
	 
	// Termino il figlio
	 kill(pidF, 12);
	 wait(&stato);
	 
 	// Termino
	 printf("\n\n");
	 printf("PARTITA TERMINATA\n\n");
	 stampa();
	 exit(1);
}

int posizione(int nTot, int *mem1) {
	int libero = 0; //trova celle vuote nella memoria1 
	int i = 2;
	while (libero == 0 && i < nTot+2) {
		if (mem1[i] == 0) {
			libero = 1;
		}
		i++;
	}
	
	if (libero != 0) {
		return i-3;
	}
	else {
		printf("Possono giocare al massimo %i giocatori\n", nTot);
		exit(1);
	}
}


//------------------------------ Main -----------------------------------------------------------------------------
int main(int argc, char *argv[]) {
	// Dichiarazione delle variabili
	 int mS_ID, *status; //id e puntatore della memoria condivisa 1
	 int mL_ID, *lanci; //id e puntatore della memoria condivisa 2
	 int mP_ID, *puntate; //id e puntatore della memoria condivisa 3
	 int sS_ID; //id semaforo 1
	 int sL_ID; //id semaforo 2
	 int p_mem1; //cella del giocatore nella memoria condivisa 1
	 int p_sem1; //cella del giocatore nel semaforo 1
	 int p_mem2;
	 int uno; //1% di soldi
	 int cinquanta; //50% di soldi
	 char *nome; //nome del giocatore
	 
	signal(2, SIG_IGN);
	
	// Controllo i parametri inseriti da riga di comando
	 if (argc != 3) {
		printf("\nErrore!\nUso: \"./gamer <nomeGiocatore> <monete>\" \n\n");
		exit(1);
	 }
	 else {
		nome = argv[1];
		sommaI = atoi(argv[2]);
		if (sommaI <= 1) {
			printf("\nErrore!\nIl giocatore deve avere più di una moneta \n\n");
			exit(1);
		}
	 }
	 
	 
	// Accedo all'area di memoria condivisa 1 (stato giocatori)
	 mS_ID = shmget(KEY1, 0, 0644);
	 status = shmat(mS_ID, NULL, 0);
	
	// Accedo all'area di memoria condivisa 2 (lanci)
	 mL_ID = shmget(KEY2, 0, 0644);
	 lanci = shmat(mL_ID, NULL, 0);
	 
	// Accedo all'area di memoria condivisa 3 (puntate)
	 mP_ID = shmget(KEY3, 0, 0644);
	 puntate = shmat(mP_ID, NULL, 0);
	
	// Accedo al semaforo 1 (gestione terminazione processi)
	 sS_ID = status[0];
	
	// Accedo al semaforo 2 (turni)
	 sL_ID = lanci[0];
	
	
	// Inserisco il nuovo giocatore
	n_giocatore = posizione(lanci[1], status);
	piu.sem_num = 0;
	semop(sL_ID, &piu, 1);
	p_mem2 = n_giocatore+3;
	
	
	// Rendo attivo il giocatore
	 p_mem1 = n_giocatore + 2;
	 status[p_mem1] = 1;
	
	
	// Creazione figlio per il controllo terminazione server
	 pidF = fork();
	 if (pidF == 0) {
	 	signal(12, termina);
	 	signal(2, SIG_IGN);
		p_sem1 = n_giocatore+1;
		meno.sem_num = p_sem1;
		semop(sS_ID, &meno, 1); //sospendo
			
		// Al risveglio
		kill(getppid(), 10);
	 }
	 else {
		// Il padre gioca
		signal(10, server_shutdown);
		signal(12, SIG_IGN);
		somma = sommaI;
		
		// Inizio del gioco
		 system("clear");
		 printf("Benvenuto %s\n", nome);
		 printf("Attendi il tuo turno per giocare...\n");
		 piu.sem_num = 1;
		 semop(sL_ID, &piu, 1); //risveglio il server
		 meno.sem_num = n_giocatore +2;
		 semop(sL_ID, &meno, 1); //addormento il giocatore in attesa di poter giocare
		 
		 
		while (1) {
			 if (somma > 1) {
			 	signal(2, SIG_IGN);
				system("clear");
				printf("NUOVA PARTITA - Partita n. %i\n", partite.giocate+1);
				 
				// Puntata
			 	 uno = somma/100;
			 	 if (uno < 1) {
			 	 	uno = 1;
			 	 }
				 cinquanta = (somma*50)/100;
				 
			 	 srand(time(NULL));
			 	 puntate[n_giocatore] = rand() %cinquanta +uno;
				 
				 printf("\n\n");
				 printf("Hai puntato %i moneta/e\n", puntate[n_giocatore]);
				 
				 piu.sem_num = 1;
				 semop(sL_ID, &piu, 1); //risveglio il server
				 
				 meno.sem_num = n_giocatore + 2;
				 semop(sL_ID, &meno, 1); //addormento il giocatore in attesa di poter lanciare i dadi
				 
				// Lancio dei dadi
				 printf("Lancio dei dadi in corso...\n");
				 srand(time(NULL));
				 lanci[p_mem2] = rand() %11 +2;
				 printf("Hai ottenuto: %i\n", lanci[p_mem2]);
				  
				 piu.sem_num = 1;
				 semop(sL_ID, &piu, 1); //risveglio il server
				  
				 meno.sem_num = n_giocatore + 2;
				 semop(sL_ID, &meno, 1); //addormento il giocatore
				  
				// Riscossione vincita
				 if (lanci[2] == 0) {
				 	printf("\n\n");
				 	printf("Ha vinto il banco\n");
				 	partite.perse++;
				 	somma -= puntate[n_giocatore];
				 }
				 else if (lanci[2] == -1) {
				 	printf("\n\n");
				 	printf("HAI VINTO\n");
				 	somma += (puntate[n_giocatore])*2;
				 	partite.vinte++;
				 }
				 else {
				 	printf("\n\n");
				 	printf("Hai perso\n");
				 	partite.perse++;
				 	somma -= puntate[n_giocatore];
				 }
				 
				signal(2, ctrlc);
				signal(15, ctrlc);
				partite.giocate++;
				
				if (somma <= 1) {
			  		// Il giocatore ha finito i soldi
			  		kill(getpid(), 15);
			  	}
				
				piu.sem_num = 1;
			  	semop(sL_ID, &piu, 1); //risveglio il server
 			    
			  	meno.sem_num = n_giocatore + 2;
			  	semop(sL_ID, &meno, 1); //addormento il giocatore
			 }
		}
		 
	 }

	return 0;
}


/*
 Matricola: VR429604
 Nome Cognome: LAURA CORSO
 Data di realizzazione: 27/05/2020
 Titolo: Secondo elaborato - UNIX System Call ~ gamer
*/
