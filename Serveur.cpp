#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/wait.h>
#include <signal.h>
#include <string.h>
#include <unistd.h>
#include <setjmp.h>
#include "Semaphore.h"
#include "protocole.h" // contient la cle et la structure d'un message

#include "FichierClient.h"

int idQ,idShm,idSem;
int idPubli, idAccesBD;
int fdPipe[2];
TAB_CONNEXIONS *tab;
sigjmp_buf env;

void HandlerSIGINT(int);
void HandlerSIGCHLD(int);

void afficheTab();

int main()
{
  // Armement des signaux

  struct sigaction A;

  A.sa_handler = HandlerSIGINT;
  sigemptyset(&A.sa_mask);
  A.sa_flags = 0 ;

  if (sigaction(SIGINT,&A,NULL) == -1)
  {
    perror("Erreur de sigaction");
    exit(1);
  }

  struct sigaction A2;

  A2.sa_handler = HandlerSIGCHLD;
  sigemptyset(&A2.sa_mask);
  A2.sa_flags = 0;

  if(sigaction(SIGCHLD,&A2,NULL) == -1)
  {
    perror("Erreur de sigaction");
    exit(1);
  }

  //Creation semaphore
  if((idSem = semget(CLE,0,0)) == -1)
  {
    if((idSem = semget(CLE,7, IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
      perror("Erreur de semget");
      exit(1);
    }
  }

  //set à 1 pour le gérant
  if(semctl(idSem,6,SETVAL,1) == -1)
  {
    perror("Erreur de semctl (2)");
    exit(1);
  }

  // Creation de la file de message
  fprintf(stderr,"(SERVEUR %d) Creation de la file de messages\n",getpid());


  if ((idQ = msgget(CLE,0)) == -1)  // CLE definie dans protocole.h
  {
    if ((idQ = msgget(CLE,IPC_CREAT | IPC_EXCL | 0600)) == -1) 
    {
      perror("(SERVEUR) Erreur de msgget");
      exit(1);
    }
  }

  //creation memoire partagée  

  if((idShm = shmget(CLE,0,0)) == -1)
  {
    if((idShm = shmget(CLE,52,IPC_CREAT | IPC_EXCL | 0600)) == -1)
    {
      perror("Erreur de shmget");
      exit(1);
    }
  }

  // Creation du pipe
  if(pipe(fdPipe) == -1)
  {
    perror("Erreur de pipe");
    exit(1);
  }

  // Initialisation du tableau de connexions
  tab = (TAB_CONNEXIONS*) malloc(sizeof(TAB_CONNEXIONS)); 

  for (int i=0 ; i<6 ; i++)
  {
    tab->connexions[i].pidFenetre = 0;
    strcpy(tab->connexions[i].nom,"");
    tab->connexions[i].pidCaddie = 0;
  }
  tab->pidServeur = getpid();
  //tab->pidPublicite = 0;


  // Creation du processus Publicite (étape 2)

  if((idPubli = fork()) == -1)
  {
    perror("Erreur de fork");
    exit(1);
  }

  if(idPubli==0)
  {
    if (execl("./Publicite","Publicite",NULL) == -1)
    {
      perror("Erreur de execl");
      exit(1);
    }
  }

  tab->pidPublicite=idPubli;


  // Creation du processus AccesBD (étape 4)

  char tmp[100];
  sprintf(tmp, "%d", fdPipe[0]);

  if((idAccesBD = fork()) == -1)
  {
    perror("Erreur de fork");
    exit(1);
  }

  if(idAccesBD==0)
  {
    if (execl("./AccesBD","AccesBD", tmp, NULL) == -1)
    {
      perror("Erreur de execl accesBD");
      exit(1);
    }
  }

  tab->pidAccesBD=idAccesBD;

  afficheTab();



  MESSAGE m;
  MESSAGE reponse;

  int affiche=0;
  int idCaddie, i, idClient;


  while(1)
  {
    if(!affiche)
  	  fprintf(stderr,"(SERVEUR %d) Attente d'une requete...\n",getpid());

    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),1,0) == -1)
    {
      perror("(SERVEUR) Erreur de msgrcv");
      msgctl(idQ,IPC_RMID,NULL);
      exit(1);
    }

    affiche = 0;

    switch(m.requete)
    {
      case CONNECT :  
                      fprintf(stderr,"(SERVEUR %d) Requete CONNECT reçue de %d\n",getpid(),m.expediteur);

                      for (i=0 ; i<6 && tab->connexions[i].pidFenetre!=0; i++);

                      if(i<6)
                        tab->connexions[i].pidFenetre=m.expediteur;
                      else
                        fprintf(stderr,"plus de place (max 6)");

                      break;

      case DECONNECT : 
                      fprintf(stderr,"(SERVEUR %d) Requete DECONNECT reçue de %d\n",getpid(),m.expediteur);

                      for (i=0 ; i<6 && tab->connexions[i].pidFenetre!= m.expediteur; i++);

                      if(i<6)
                        tab->connexions[i].pidFenetre=0;
                      else
                        fprintf(stderr,"pas trouvé");
                        
                      break;
      case LOGIN :    
                      fprintf(stderr,"(SERVEUR %d) Requete LOGIN reçue de %d : --%d--%s--%s--\n",getpid(),m.expediteur,m.data1,m.data2,m.data3);
                      
                      //verification gerant connecté(maintenance)

                      idClient=m.expediteur;
                      
                      if(semctl(idSem,6,GETVAL) == 0)
                      {
                        m.type=idClient;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }

                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }

                      

                      //verification login
                        
                      int posi, login;
                      char log[50];
                      login=0;

                      strcpy(log, "login reussi !");
                      posi=estPresent(m.data2);
                      

                      if(posi==-1)
                      {
                        fprintf(stderr,"Erreur lors de l'ouverture du fichier");
                        exit(1);   
                      }

                      if(m.data1)//nouveau client coché
                      {
                        if(posi>0)
                        {
                          strcpy(log, "Client deja existant");
                          login=1;
                        }
                        else
                        {
                          ajouteClient(m.data2, m.data3);
                        }
                      }
                      else//nouveau client pas coché
                      {
                        //verification client deja connecté
                        for (i=0 ; i<6 && strcmp(tab->connexions[i].nom, m.data2)!=0; i++);

                        if(i!=6)
                        {
                          strcpy(log, "Ce client est deja connecte");
                          login=1;
                        }

                        if(login==0 && estPresent(m.data2)==0)
                        {
                          strcpy(log, "Client pas trouve");
                          login=1;
                        }

                        if(login==0 && verifieMotDePasse(posi, m.data3)==-1)
                        {
                          fprintf(stderr,"Erreur lors de l'ouverture du fichier");
                          exit(1);   
                        }
                        else if(login==0 && !verifieMotDePasse(posi, m.data3))
                        {
                          strcpy(log, "MDP incorrect");
                          login=1;
                        }
                        
                      }

                      if(login==0)// login reussi
                      {
                        //creation processus caddie
                        if((idCaddie = fork()) == -1)
                        {
                          perror("Erreur de fork");
                          exit(1);
                        }
                        if(idCaddie==0)
                        {
                          sprintf(tmp, "%d", fdPipe[1]);
                          if (execl("./Caddie","Caddie", tmp, NULL) == -1)
                          {
                            perror("Erreur de execl");
                            exit(1);
                          }
                        }

                        //Login au caddie pour lui dire a quel client il est connecte
                        reponse.expediteur=idClient;
                        reponse.type=idCaddie;
                        reponse.requete=LOGIN;

                        for (i=0 ; i<6 && tab->connexions[i].pidFenetre!=idClient; i++);

                        //numero semaphore(pas demandé)
                        reponse.data5=i;

                        if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }

                        
                        //mettre pidCaddie au bon client + nom

                        for (i=0 ;tab->connexions[i].pidFenetre!=idClient; i++);

                        strcpy(tab->connexions[i].nom, m.data2);
                        tab->connexions[i].pidCaddie=idCaddie;

                        //prepa message client (reussi)

                        reponse.data1=1;
                        strcpy(reponse.data4, log);
                        
                      }
                      else//login echoué
                      {
                        reponse.data1=0;
                        strcpy(reponse.data4, log);
                      } 

                      //envoie reponse au client

                      reponse.type=idClient;
                      reponse.expediteur=getpid();
                      reponse.requete=LOGIN;

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd serv");
                        exit(1);
                      }
                      if(kill(m.expediteur,SIGUSR1) == -1)
                      {
                        perror ("Erreur de kill");
                        exit(1);
                      }                      
                                        
                      break; 

      case LOGOUT :  
                      fprintf(stderr,"(SERVEUR %d) Requete LOGOUT reçue de %d\n",getpid(),m.expediteur);
                      
                      //verification gerant connecté(maintenance)
                      if (semctl(idSem,6,GETVAL) == 0)
                      {
                        
                        m.type=m.expediteur;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }

                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }
                      for (i=0 ; i<6 && tab->connexions[i].pidFenetre!= m.expediteur; i++);

                      if(i<6)
                      {
                        strcpy(tab->connexions[i].nom, "");
                      }
                      else
                        fprintf(stderr,"pas trouvé");//normalement impossible

                      reponse.expediteur=getpid();
                      reponse.type=tab->connexions[i].pidCaddie;
                      reponse.requete=LOGOUT;

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd(LOGOUT)");
                        exit(1);
                      }
                        
                      break;

      case UPDATE_PUB :
                        idPubli=m.expediteur;
                        for (i=0 ; i<6 && tab->connexions[i].pidFenetre!=0; i++)
                        {
                          if(kill(tab->connexions[i].pidFenetre,SIGUSR2) == -1)
                          {
                            perror ("Erreur de kill");
                            exit(1);
                          }         
                        }

                        affiche=1;

                      
                        break;

      case CONSULT :  
                      fprintf(stderr,"(SERVEUR %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      //verification gerant connecté(maintenance)
                      if (semctl(idSem,6,GETVAL) == 0)
                      {
                        
                        m.type=m.expediteur;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }
                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }

                      for (i=0 ;tab->connexions[i].pidFenetre!=m.expediteur; i++);

                      reponse.type=tab->connexions[i].pidCaddie;
                      reponse.expediteur=m.expediteur;
                      reponse.requete=CONSULT;
                      reponse.data1=m.data1;

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd CONSULT");
                        exit(1);
                      }                      

                      break;

      case ACHAT :    // TO DO
                      fprintf(stderr,"(SERVEUR %d) Requete ACHAT reçue de %d\n",getpid(),m.expediteur);
                      //verification gerant connecté(maintenance)
                      if (semctl(idSem,6,GETVAL) == 0)
                      {
                        
                        m.type=m.expediteur;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }
                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }
                      for (i=0 ; tab->connexions[i].pidFenetre!=m.expediteur ; i++);

                      m.type=tab->connexions[i].pidCaddie;

                      if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd CONSULT");
                        exit(1);
                      }      

                      break;

      case CADDIE :   
                      fprintf(stderr,"(SERVEUR %d) Requete CADDIE reçue de %d\n",getpid(),m.expediteur);
                      //verification gerant connecté(maintenance)
                      if (semctl(idSem,6,GETVAL) == 0)
                      {
                        
                        m.type=m.expediteur;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }
                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }
                      for (i=0 ; tab->connexions[i].pidFenetre!=m.expediteur ; i++);

                      m.type=tab->connexions[i].pidCaddie;
                      m.expediteur=getpid();//ou inchangé
                      m.requete=CADDIE;

                      if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd CONSULT");
                        exit(1);
                      }      

                      break;

      case CANCEL :   
                      fprintf(stderr,"(SERVEUR %d) Requete CANCEL reçue de %d\n",getpid(),m.expediteur);
                      //verification gerant connecté(maintenance)
                      if (semctl(idSem,6,GETVAL) == 0)
                      {
                        m.type=m.expediteur;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }
                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }                     
                      for (i=0 ; tab->connexions[i].pidFenetre!=m.expediteur ; i++);

                      reponse.type=tab->connexions[i].pidCaddie;
                      reponse.requete=CANCEL;
                      reponse.data1=m.data1;

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd CANCEL");
                        exit(1);
                      }  
                      
                      break;

      case CANCEL_ALL :
                      fprintf(stderr,"(SERVEUR %d) Requete CANCEL_ALL reçue de %d\n",getpid(),m.expediteur);
                      //verification gerant connecté(maintenance)
                      if (semctl(idSem,6,GETVAL) == 0)
                      {
                        m.type=m.expediteur;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }
                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }
                      for (i=0 ; tab->connexions[i].pidFenetre!=m.expediteur ; i++);

                      reponse.type=tab->connexions[i].pidCaddie;
                      reponse.requete=CANCEL_ALL;

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd CANCEL_ALL");
                        exit(1);
                      }  
                      break;

      case PAYER : 
                      fprintf(stderr,"(SERVEUR %d)Requete PAYER reçue de %d\n",getpid(),m.expediteur);
                      //verification gerant connecté(maintenance)
                      if(semctl(idSem,6,GETVAL) == 0)
                      {
                        
                        m.type=m.expediteur;
                        m.expediteur=getpid();
                        m.requete=BUSY;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd serv");
                          exit(1);
                        }
                        if(kill(m.type,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        } 

                        break;
                      }
                      for(i=0 ; tab->connexions[i].pidFenetre!=m.expediteur ; i++);

                      reponse.type=tab->connexions[i].pidCaddie;
                      reponse.requete=PAYER;

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd PAYER");
                        exit(1);
                      }  

                      break;

      case NEW_PUB : 
                      fprintf(stderr,"(SERVEUR %d) Requete NEW_PUB reçue de %d\n",getpid(),m.expediteur);

                      m.type=idPubli;
                      m.expediteur=getpid();

                      if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd PAYER");
                        exit(1);
                      }  
                      if(kill(idPubli,SIGUSR1) == -1)
                      {
                        perror ("Erreur de kill");
                        exit(1);
                      } 

                      break;
    }

    sigsetjmp(env, 1);

    if(!affiche)
      afficheTab();
  }
}

void afficheTab()
{
  fprintf(stderr,"Pid Serveur   : %d\n",tab->pidServeur);
  fprintf(stderr,"Pid Publicite : %d\n",tab->pidPublicite);
  fprintf(stderr,"Pid AccesBD   : %d\n",tab->pidAccesBD);
  for (int i=0 ; i<6 ; i++)
    fprintf(stderr,"%6d -%20s- %6d\n",tab->connexions[i].pidFenetre,
                                                      tab->connexions[i].nom,
                                                      tab->connexions[i].pidCaddie);
  fprintf(stderr,"\n");
}

void HandlerSIGINT(int sig)
{

  
  fprintf(stderr,"\n(Traitement %d) Reception du signal (%d)\n",getpid(),sig);


  MESSAGE m;

  m.expediteur=getpid();
  m.requete=LOGOUT;
  int i;
  int idClient;



  fprintf(stderr,"(SERVEUR)EXIT DES CLIENT(S) et CADDIE(S) CONNECTES\n");

  for (i=0 ; i<6; i++)
  {
    if(tab->connexions[i].pidCaddie != 0)
    {
      m.type=tab->connexions[i].pidCaddie;

      if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
      {
        perror("Erreur de msgsnd LOGOUT");
        exit(1);
      }  
    }
    if(tab->connexions[i].pidFenetre != 0)
    {
      idClient=tab->connexions[i].pidFenetre;
      if(kill(idClient, SIGTERM) == -1)
      {
        perror ("Erreur de kill");
        exit(1);
      }          
    }
  }


  //exit publi

  fprintf(stderr,"(SERVEUR)EXIT PUBLICITE\n");


  if (kill(idPubli, SIGTERM) == -1)
  {
    perror("Error sending SIGTERM signal to Publicite process");
    exit(1);
  }

  //exit AccesBD

  m.type=idAccesBD;
  m.requete=EXIT;

  int ret;

  fflush(stdout);

  if((ret = write(fdPipe[1], &m, sizeof(MESSAGE)-sizeof(long))) != sizeof(MESSAGE)-sizeof(long))
  {
    perror("Erreur de write (1)");
    exit(1);
  }

  fprintf(stderr,"(SERVEUR)SUPPRESSION FILE DE MESSAGE\n");


  if(msgctl(idQ,IPC_RMID,NULL) == -1)
  {
    perror("Erreur de msgctl");
    exit(1);
  }

  fprintf(stderr,"(SERVEUR)SUPPRESSION ENSEMBLE DE SEMAPHORE\n");

  if(semctl(idSem, 0, IPC_RMID) == -1)
  {
    perror("Erreur de semctl (2)");
    exit(1);
  }


  fprintf(stderr,"(SERVEUR)SUPPRESSION MEMOIRE PARTAGEE\n");


  if(shmctl(idShm,IPC_RMID,NULL) == -1)
  {
    perror("Erreur de shmctl");
    exit(1);
  }

  fprintf(stderr,"(SERVEUR)FERMETURE PIPE\n");

  if(close(fdPipe[0]) == -1)
  {
    perror("Erreur fermeture sortie du pipe");
    exit(1);
  }
  if(close(fdPipe[1]) == -1)
  {
    perror("Erreur fermeture sortie du pipe");
    exit(1);
  }

  exit(0);
}

void HandlerSIGCHLD(int sig)
{

  fprintf(stderr,"(Traitement %d) Reception du signal (%d)\n",getpid(),sig);

  int id, status;
  id = wait(&status);
  printf("\n(PERE) Suppression du fils %d de la table des processus\n",id);

  int i;

  for (i=0 ; i<6 && tab->connexions[i].pidCaddie != id; i++);

  if(i<6)
  {
    tab->connexions[i].pidCaddie=0;
    strcpy(tab->connexions[i].nom, "");
  }
  else
    fprintf(stderr,"pas trouvé");

  siglongjmp(env, 1);
     
}



