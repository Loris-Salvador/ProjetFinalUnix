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
  MYSQL* connexion;

  connexion = mysql_init(NULL);
  if(mysql_real_connect(connexion,"localhost","Student","PassStudent1_","PourStudent",0,0,0) == NULL)
  {
    fprintf(stderr,"(SERVEUR) Erreur de connexion à la base de données...\n");
    exit(1);  
  }




  MESSAGE m;
  MESSAGE reponse;

  MYSQL_ROW ligne;

  char requete[200];


  MYSQL_RES  *resultat;

  int test;
  int i;

  while(1)
  {
    // Lecture d'une requete sur le pipe
    int ret;

    if((ret = read(fdRpipe,&m, sizeof(MESSAGE)-sizeof(long))) < 0)
    {
      perror("Erreur de read");
      exit(1);
    }

    test=0;

    switch(m.requete)
    {
      case CONSULT : 
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


                      while ((ligne = mysql_fetch_row(resultat)) != NULL && atoi(ligne[0]) != m.data1);//recherche du bon article en fct de l'id
       
                      
                      if(atoi(ligne[0]) == m.data1)
                      {
                        reponse.data1=atoi(ligne[0]);
                        strcpy(reponse.data2, ligne[1]);
                        strcpy(reponse.data3, ligne[3]);
                        strcpy(reponse.data4, ligne[4]);
                        reponse.data5=atof(ligne[2]); //bizarre ca devrait pas marcher par rapport a gerant
                      }
                      else
                      {
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

      case ACHAT :  
                      fprintf(stderr,"(ACCESBD %d) Requete ACHAT reçue de %d\n",getpid(),m.expediteur);
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

                      i=0;

                      while (test==0 && (ligne = mysql_fetch_row(resultat)) != NULL) 
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

                          mysql_data_seek(resultat, (i-1));
                          
                          ligne = mysql_fetch_row(resultat);

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

                  
                        mysql_free_result(resultat);    

                        if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("(ACCESBD ACHAT)Erreur de msgsnd");
                          exit(1);
                        } 
                      }

                      break;

      case CANCEL : 
                      fprintf(stderr,"(ACCESBD %d) Requete CANCEL reçue de %d\n",getpid(),m.expediteur);
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

                      // Mise à jour du stock en BD

                      test=0;

                      while (test==0 && (ligne = mysql_fetch_row(resultat)) != NULL) 
                      {
                        
                        if(atoi(ligne[0]) == m.data1)
                        {
                          test=1;
                        }
                      }                  

                      if(test==1)
                      {
                        int stock;
                        stock=atoi(ligne[3]);
                        int stock2;
                        stock2=stock+atoi(m.data2);


                        sprintf(requete,"update UNIX_FINAL set stock=%d where id=%d", stock2, atoi(ligne[0]));
                
                        if(mysql_query(connexion,requete) != 0)
                        {
                          fprintf(stderr, "Erreur de mysql_query: %s\n",mysql_error(connexion));
                          exit(1);
                        }    
                      }                 


                      break;

      case EXIT   :   
                    fprintf(stderr,"(ACCESBD %d) Requete EXIT reçue de %d\n",getpid(),m.expediteur);
                    
                    mysql_close(connexion);
                        
                    fprintf(stderr,"(ACCESBD) Base de donnee fermee\n");
                    exit(0);


    }
  }
}
