#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <signal.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include "protocole.h" // contient la cle et la structure d'un message

int idQ, idShm;
char *pShm;
void handlerSIGUSR1(int sig);

int fd;

int main()
{
  // Armement des signaux
  // TO DO
  // struct sigaction A;
  // A.sa_handler =handlerSIGUSR1;
  // sigemptyset(&A.sa_mask);
  // A.sa_flags = 0;

  // if(sigaction(SIGUSR1,&A,NULL) == -1)
  // {
  //   perror("Erreur de sigaction");
  //   exit(1);
  // }

  //Masquage des signaux
  sigset_t mask;
  sigfillset(&mask);
  sigdelset(&mask,SIGUSR1);
  sigdelset(&mask,SIGTERM);
  sigprocmask(SIG_SETMASK,&mask,NULL);

  // Recuperation de l'identifiant de la file de messages
  fprintf(stderr,"\n(PUBLICITE %d) Recuperation de l'id de la file de messages\n",getpid());
  if ((idQ = msgget(CLE,0)) == -1)
  {
    perror("(PUBLICITE) Erreur de msgget");
    exit(1);
  }

  // Recuperation de l'identifiant de la mémoire partagée

  if((idShm = shmget(CLE,0,0)) == -1)
  {
    perror("Erreur de shmget");
    exit(1);
  }

  // Attachement à la mémoire partagée

  if ((pShm = (char*)shmat(idShm,NULL,0)) == (char*)-1)
  {
    perror("Erreur de shmat");
    exit(1);
  } 

  // Mise en place de la publicité en mémoire partagée
  char pub[51];
  strcpy(pub,"Bienvenue sur le site du Maraicher en ligne !");

  for (int i=0 ; i<=50 ; i++) pShm[i] = ' ';
  pShm[51] = '\0';
  int indDebut = 25 - strlen(pub)/2;
  for (int i=0 ; i<strlen(pub) ; i++) pShm[indDebut + i] = pub[i];

  MESSAGE m;
  m.requete=UPDATE_PUB;
  m.expediteur=getpid();
  m.type=1;

  while(1)
  {
    // Envoi d'une requete UPDATE_PUB au serveur

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd PUB");
      exit(1);
    }

    sleep(1); 

    // Decallage vers la gauche

    char tmp;
    tmp=pShm[0];

    for(int i=0;i<50;i++)
    {
      pShm[i]=pShm[i+1];
    }

    pShm[50]=tmp;

  }
}

void handlerSIGUSR1(int sig)
{
  fprintf(stderr,"(PUBLICITE %d) Nouvelle publicite !\n",getpid());

  // Lecture message NEW_PUB

  // Mise en place de la publicité en mémoire partagée
}
