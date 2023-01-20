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
#include <mysql.h>
#include "protocole.h" // contient la cle et la structure d'un message

int idQ;

ARTICLE articles[10];
int nbArticles = 0;

int fdWpipe;
int pidClient;

MYSQL* connexion;

void handlerSIGALRM(int sig);

int main(int argc,char* argv[])
{
  // Masquage de SIGINT
  sigset_t mask;
  sigaddset(&mask,SIGINT);
  sigprocmask(SIG_SETMASK,&mask,NULL);

  // Armement des signaux
  // TO DO

  // Recuperation de l'identifiant de la file de messages
  fprintf(stderr,"(CADDIE %d) Recuperation de l'id de la file de messages\n",getpid());
  if ((idQ = msgget(CLE,0)) == -1)
  {
    perror("(CADDIE) Erreur de msgget");
    exit(1);
  }

  // Connexion à la base de donnée etape 4
  // connexion = mysql_init(NULL);
  // if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  // {
  //   fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
  //   exit(1);  
  // }


  MESSAGE m;
  MESSAGE reponse;
  
  char requete[200];
  char newUser[20];
  MYSQL_RES  *resultat;
  MYSQL_ROW  Tuple;

  int i;
  int test,same;
  int ret;

  // Récupération descripteur écriture du pipe
  fdWpipe = atoi(argv[1]);


  while(1)
  {
    if (msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),getpid(),0) == -1)
    {
      perror("(CADDIE) Erreur de msgrcv");
      exit(1);
    }

    test=0;
    same=0;

    switch(m.requete)
    {
      case LOGIN :    // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete LOGIN reçue de %d\n",getpid(),m.expediteur);
                      
                      pidClient=m.expediteur;

                      break;

      case LOGOUT :   // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete LOGOUT reçue de %d\n",getpid(),m.expediteur);
                      //mysql_close(connexion);
                      exit(0);
                      break;

      case CONSULT :  // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                  
                      m.expediteur=getpid();

                      fflush(stdout);//utilité??

                      if ((ret = write(fdWpipe, &m, sizeof(MESSAGE)-sizeof(long))) != sizeof(MESSAGE)-sizeof(long))
                      {
                        perror("Erreur de write (1)");
                        exit(1);
                      }


                      if(msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),getpid(),0) == -1)
                      {
                        perror("(CADDIE) Erreur de msgrcv");
                        exit(1);
                      }
                      //envoyer au client

                      if(m.data1 != -1)
                      {
                          m.type=pidClient;

                          if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                          {
                            perror("Erreur de msgsnd");
                            exit(1);
                          } 


                          if(kill(pidClient,SIGUSR1) == -1)
                          {
                            perror ("Erreur de kill");
                            exit(1);
                          } 
                      }
                 
                      break;

      case ACHAT :    // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete ACHAT reçue de %d\n",getpid(),m.expediteur);

                      // on transfert la requete à AccesBD

                      m.expediteur=getpid();//?????

                      fflush(stdout);//utilité??

                      if ((ret = write(fdWpipe, &m, sizeof(MESSAGE)-sizeof(long))) != sizeof(MESSAGE)-sizeof(long))
                      {
                        perror("Erreur de write (1)");
                        exit(1);
                      }
                      
                      // on attend la réponse venant de AccesBD

                      if(msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),getpid(),0) == -1)
                      {
                        perror("(CADDIE) Erreur de msgrcv");
                        exit(1);
                      }
                      ///etape 5

                      if(strcmp(m.data3, "0")!=0)
                      {

                        for(i=0; articles[i].id!=m.data1 && i<10 ; i++);

                        if(i!=10)
                        {
                          articles[i].stock=articles[i].stock+atoi(m.data3);
                          same=1;
                        }

                        if(same==0)// condition si le meme article on rajoute dedans
                        {
                          for(i=0; articles[i].id!=0 && i<10 ; i++); 
                        
                         

                          if(i!=10)
                          {
                            articles[i].id=m.data1;
                            strcpy(articles[i].intitule, m.data2);
                            articles[i].stock=atoi(m.data3);
                            articles[i].prix=m.data5;
                            strcpy(articles[i].image, m.data4);
                            nbArticles++;
                          }
                          else
                          {
                            strcpy(m.data3, "-1");
                          }
                          
                        }
                      }
                                          
                      // Envoi de la reponse au client

                      m.type=pidClient;

                      if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd");
                        exit(1);
                      } 

                      if(kill(pidClient,SIGUSR1) == -1)
                      {
                        perror ("Erreur de kill");
                        exit(1);
                      }                      
                              
                      break;

      case CADDIE :   // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete CADDIE reçue de %d\n",getpid(),m.expediteur);

                      reponse.type=pidClient;
                      reponse.expediteur=getpid();
                      reponse.requete=CADDIE;

                      for(i=0 ; i<nbArticles ;i++)
                      {


                       usleep(5000);

                        reponse.data1=articles[i].id;
                        strcpy(reponse.data2, articles[i].intitule);
                        sprintf(reponse.data3, "%d", articles[i].stock);
                        strcpy(reponse.data4, articles[i].image);
                        reponse.data5=articles[i].prix;

                        if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd");
                          exit(1);
                        } 

                        if(kill(pidClient,SIGUSR1) == -1)
                        {
                          perror ("Erreur de kill");
                          exit(1);
                        }        

                      }
                      break;

      case CANCEL :   // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete CANCEL reçue de %d\n",getpid(),m.expediteur);

                      // on transmet la requete à AccesBD

                      // Suppression de l'aricle du panier
                      break;

      case CANCEL_ALL : // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete CANCEL_ALL reçue de %d\n",getpid(),m.expediteur);

                      // On envoie a AccesBD autant de requeres CANCEL qu'il y a d'articles dans le panier

                      // On vide le panier
                      break;

      case PAYER :    // TO DO
                      fprintf(stderr,"(CADDIE %d) Requete PAYER reçue de %d\n",getpid(),m.expediteur);

                      // On vide le panier
                      break;
      case EXIT  :    
                      fprintf(stderr,"(CADDIE %d) Requete EXIT reçue de %d\n",getpid(),m.expediteur);
                      exit(0);

                      break;

    }
  }
}

void handlerSIGALRM(int sig)
{
  fprintf(stderr,"(CADDIE %d) Time Out !!!\n",getpid());

  // Annulation du caddie et mise à jour de la BD
  // On envoie a AccesBD autant de requetes CANCEL qu'il y a d'articles dans le panier

  // Envoi d'un Time Out au client (s'il existe toujours)
         
  exit(0);
}