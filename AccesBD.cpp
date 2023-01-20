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
#include <cerrno>
#include <unistd.h>
#include <signal.h>
#include <mysql.h>
#include "protocole.h" // contient la cle et la structure d'un message

int idQ;

int main(int argc,char* argv[])
{
  // Masquage de SIGINT
  sigset_t mask;
  sigaddset(&mask,SIGINT);
  sigprocmask(SIG_SETMASK,&mask,NULL);

  // Recuperation de l'identifiant de la file de messages
  fprintf(stderr,"(ACCESBD %d) Recuperation de l'id de la file de messages\n",getpid());
  if ((idQ = msgget(CLE,0)) == -1)
  {
    perror("(ACCESBD) Erreur de msgget");
    exit(1);
  }

  // Récupération descripteur lecture du pipe
  int fdRpipe = atoi(argv[1]);

  // Connexion à la base de donnée
  // TO DO
  MYSQL* connexion;

  connexion = mysql_init(NULL);
  if (mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  {
    fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
    exit(1);  
  }




  MESSAGE m;
  MESSAGE reponse;

  MYSQL_ROW ligne;

  char requete[200];

  /////////////////////////////////


  MYSQL_RES  *resultat;
  MYSQL_RES  *resultat2;  

  // sprintf(requete,"select * from UNIX_FINAL");
  // if(mysql_query(connexion,requete) != 0)
  // {
  //   fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
  //   exit(1);
  // }

  // if((resultat = mysql_store_result(connexion))==NULL)
  // {
  //   fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
  //   exit(1);
  // }
  /////////////////////////////////

  int test;
  int i;

  while(1)
  {
    // Lecture d'une requete sur le pipe
    // TO DO
    int ret;

    if((ret = read(fdRpipe,&m, sizeof(MESSAGE)-sizeof(long))) < 0)
    {
      perror("Erreur de read");
      exit(1);
    }

    test=0;

    switch(m.requete)
    {
      case CONSULT :  // TO DO
                      fprintf(stderr,"(ACCESBD %d) Requete CONSULT reçue de %d\n",getpid(),m.expediteur);
                      // Acces BD
              
                      sprintf(requete,"select * from UNIX_FINAL");
                      if(mysql_query(connexion,requete) != 0)
                      {
                        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
                        exit(1);
                      }

                      if((resultat = mysql_store_result(connexion))==NULL)
                      {
                        fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
                        exit(1);
                      }


                      while (test==0 && (ligne = mysql_fetch_row(resultat)) != NULL) 
                      {
                        if(atoi(ligne[0]) == m.data1)
                        {
                          test=1;
                        }
                      }          

                      //mysql_free_result(resultat);    
                      
                      if(test==1)
                      {
                        reponse.data1=atoi(ligne[0]);
                        strcpy(reponse.data2, ligne[1]);
                        strcpy(reponse.data3, ligne[3]);
                        strcpy(reponse.data4, ligne[4]);
                        reponse.data5=atof(ligne[2]);
                      }
                      else
                      {
                        //pas trouve
                        reponse.data1=-1;
                      }         

                      //envoie message au caddie

                      reponse.expediteur=getpid();
                      reponse.type=m.expediteur;
                      reponse.requete=CONSULT;


                  
                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("(CADDIE CONSULT)Erreur de msgsnd");
                        exit(1);
                      } 

                      mysql_free_result(resultat);    


                      break;

      case ACHAT :    // TO DO
                      fprintf(stderr,"(ACCESBD %d) Requete ACHAT reçue de %d\n",getpid(),m.expediteur);
                      // Acces BD

                      sprintf(requete,"select * from UNIX_FINAL");
                      if(mysql_query(connexion,requete) != 0)
                      {
                        fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
                        exit(1);
                      }
                      if((resultat2 = mysql_store_result(connexion))==NULL)
                      {
                        fprintf(stderr, "Erreur de mysql_store_result: %s\n",mysql_error(connexion));
                        exit(1);
                      }

                      i=0;

                      while (test==0 && (ligne = mysql_fetch_row(resultat2)) != NULL) 
                      {
                        
                        if(atoi(ligne[0]) == m.data1)
                        {
                          test=1;
                        }
                        i++;
                      }                  

                      if(test==1)
                      {
                        int stock=atoi(ligne[3]);
                        int quantite=atoi(m.data2);

                        if(quantite<=stock)
                        {
                          sprintf(requete,"update UNIX_FINAL set stock=%d where id=%d", stock-quantite, atoi(ligne[0]));
                 
                          if(mysql_query(connexion,requete) != 0)
                          {
                            fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
                            exit(1);
                          }

                          mysql_data_seek(resultat2, (i-1));
                          
                          ligne = mysql_fetch_row(resultat2);

                          strcpy(reponse.data3, m.data2);

                        }
                        else
                        {
                          strcpy(reponse.data3, "0");
                        }

                        reponse.data1=atoi(ligne[0]);
                        strcpy(reponse.data2, ligne[1]);
                        strcpy(reponse.data4, ligne[4]);
                        reponse.data5=atof(ligne[2]);       

                        reponse.expediteur=getpid();
                        reponse.requete=ACHAT;
                        reponse.type=m.expediteur;

                  
                        mysql_free_result(resultat2);    

                        if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("(ACCESBD ACHAT)Erreur de msgsnd");
                          exit(1);
                        } 
                      }

                      break;

      case CANCEL :   // TO DO
                      fprintf(stderr,"(ACCESBD %d) Requete CANCEL reçue de %d\n",getpid(),m.expediteur);
                      // Acces BD

                      // Mise à jour du stock en BD
                      break;

      case EXIT   :   
                    fprintf(stderr,"(ACCESBD %d) Requete EXIT reçue de %d\n",getpid(),m.expediteur);
                    
                    // mysql_free_result(resultat);
                    // mysql_free_result(resultat2);
                    mysql_close(connexion);
                        
                    fprintf(stderr,"(ACCESBD) Base de donnee fermee\n");
                    exit(0);


    }
  }
}
