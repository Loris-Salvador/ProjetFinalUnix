#include "windowclient.h"
#include "ui_windowclient.h"
#include <QMessageBox>
#include <string>
using namespace std;

#include "protocole.h"

#include <sys/types.h>
#include <unistd.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <sys/shm.h>
#include "Semaphore.h"
#include <signal.h>
#include <sys/sem.h>

extern WindowClient *w;

int idSem;

int numSem;

int idQ, idShm;
bool logged=false;
char* pShm;
ARTICLE articleEnCours;
float totalCaddie = 0.0;

void handlerSIGUSR1(int sig);
void handlerSIGUSR2(int sig);

#define REPERTOIRE_IMAGES "images/"

WindowClient::WindowClient(QWidget *parent) : QMainWindow(parent), ui(new Ui::WindowClient)
{
    ui->setupUi(this);

    // Configuration de la table du panier (ne pas modifer)
    ui->tableWidgetPanier->setColumnCount(3);
    ui->tableWidgetPanier->setRowCount(0);
    QStringList labelsTablePanier;
    labelsTablePanier << "Article" << "Prix à l'unité" << "Quantité";
    ui->tableWidgetPanier->setHorizontalHeaderLabels(labelsTablePanier);
    ui->tableWidgetPanier->setSelectionMode(QAbstractItemView::SingleSelection);
    ui->tableWidgetPanier->setSelectionBehavior(QAbstractItemView::SelectRows);
    ui->tableWidgetPanier->horizontalHeader()->setVisible(true);
    ui->tableWidgetPanier->horizontalHeader()->setDefaultSectionSize(160);
    ui->tableWidgetPanier->horizontalHeader()->setStretchLastSection(true);
    ui->tableWidgetPanier->verticalHeader()->setVisible(false);
    ui->tableWidgetPanier->horizontalHeader()->setStyleSheet("background-color: lightyellow");

    // Recuperation de l'identifiant de la file de messages
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la file de messages\n",getpid());
    // TO DO

    if((idQ = msgget(CLE,0)) == -1)
    {
      perror("(CLIENT) Erreur de msgget");
      exit(1);
    }

    //Semaphore

    if((idSem = semget(CLE,0,0)) == -1)
    {
      perror("Erreur de semget");
      exit(1);
    }



    // Recuperation de l'identifiant de la mémoire partagée
    fprintf(stderr,"(CLIENT %d) Recuperation de l'id de la mémoire partagée\n",getpid());

    if((idShm = shmget(CLE,0,0)) == -1)
    {
      perror("Erreur de shmget");
      exit(1);
    }

    // Attachement à la mémoire partagée

    if((pShm = (char*)shmat(idShm,NULL,0)) == (char*)-1)
    {
      perror("Erreur de shmat");
      exit(1);
    } 

    // Armement des signaux

    struct sigaction A;
    A.sa_handler =handlerSIGUSR1;
    sigemptyset(&A.sa_mask);
    A.sa_flags = 0;

    if(sigaction(SIGUSR1,&A,NULL) == -1)
    {
      perror("Erreur de sigaction");
      exit(1);
    }

    struct sigaction A2;
    A2.sa_handler =handlerSIGUSR2;
    sigemptyset(&A2.sa_mask);
    A2.sa_flags = 0;

    if(sigaction(SIGUSR2,&A2,NULL) == -1)
    {
      perror("Erreur de sigaction");
      exit(1);
    }

    // Envoi d'une requete de connexion au serveur

    MESSAGE M;

    M.expediteur=getpid();
    M.requete=CONNECT;
    M.type=1;

    if(msgsnd(idQ,&M,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }
}

WindowClient::~WindowClient()
{
    delete ui;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles : ne pas modifier /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setNom(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditNom->clear();
    return;
  }
  ui->lineEditNom->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getNom()
{
  strcpy(nom,ui->lineEditNom->text().toStdString().c_str());
  return nom;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setMotDePasse(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditMotDePasse->clear();
    return;
  }
  ui->lineEditMotDePasse->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
const char* WindowClient::getMotDePasse()
{
  strcpy(motDePasse,ui->lineEditMotDePasse->text().toStdString().c_str());
  return motDePasse;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setPublicite(const char* Text)
{
  if (strlen(Text) == 0 )
  {
    ui->lineEditPublicite->clear();
    return;
  }
  ui->lineEditPublicite->setText(Text);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setImage(const char* image)
{
  // Met à jour l'image
  char cheminComplet[80];
  sprintf(cheminComplet,"%s%s",REPERTOIRE_IMAGES,image);
  QLabel* label = new QLabel();
  label->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
  label->setScaledContents(true);
  QPixmap *pixmap_img = new QPixmap(cheminComplet);
  label->setPixmap(*pixmap_img);
  label->resize(label->pixmap()->size());
  ui->scrollArea->setWidget(label);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WindowClient::isNouveauClientChecked()
{
  if (ui->checkBoxNouveauClient->isChecked()) return 1;
  return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setArticle(const char* intitule,float prix,int stock,const char* image)
{
  ui->lineEditArticle->setText(intitule);
  if (prix >= 0.0)
  {
    char Prix[20];
    sprintf(Prix,"%.2f",prix);
    ui->lineEditPrixUnitaire->setText(Prix);
  }
  else ui->lineEditPrixUnitaire->clear();
  if (stock >= 0)
  {
    char Stock[20];
    sprintf(Stock,"%d",stock);
    ui->lineEditStock->setText(Stock);
  }
  else ui->lineEditStock->clear();
  setImage(image);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WindowClient::getQuantite()
{
  return ui->spinBoxQuantite->value();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::setTotal(float total)
{
  if (total >= 0.0)
  {
    char Total[20];
    sprintf(Total,"%.2f",total);
    ui->lineEditTotal->setText(Total);
  }
  else ui->lineEditTotal->clear();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::loginOK()
{
  ui->pushButtonLogin->setEnabled(false);
  ui->pushButtonLogout->setEnabled(true);
  ui->lineEditNom->setReadOnly(true);
  ui->lineEditMotDePasse->setReadOnly(true);
  ui->checkBoxNouveauClient->setEnabled(false);

  ui->spinBoxQuantite->setEnabled(true);
  ui->pushButtonPrecedent->setEnabled(true);
  ui->pushButtonSuivant->setEnabled(true);
  ui->pushButtonAcheter->setEnabled(true);
  ui->pushButtonSupprimer->setEnabled(true);
  ui->pushButtonViderPanier->setEnabled(true);
  ui->pushButtonPayer->setEnabled(true);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::logoutOK()
{
  ui->pushButtonLogin->setEnabled(true);
  ui->pushButtonLogout->setEnabled(false);
  ui->lineEditNom->setReadOnly(false);
  ui->lineEditMotDePasse->setReadOnly(false);
  ui->checkBoxNouveauClient->setEnabled(true);

  ui->spinBoxQuantite->setEnabled(false);
  ui->pushButtonPrecedent->setEnabled(false);
  ui->pushButtonSuivant->setEnabled(false);
  ui->pushButtonAcheter->setEnabled(false);
  ui->pushButtonSupprimer->setEnabled(false);
  ui->pushButtonViderPanier->setEnabled(false);
  ui->pushButtonPayer->setEnabled(false);

  setNom("");
  setMotDePasse("");
  ui->checkBoxNouveauClient->setCheckState(Qt::CheckState::Unchecked);

  setArticle("",-1.0,-1,"");

  w->videTablePanier();
  totalCaddie = 0.0;
  w->setTotal(-1.0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions utiles Table du panier (ne pas modifier) /////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::ajouteArticleTablePanier(const char* article,float prix,int quantite)
{
    char Prix[20],Quantite[20];

    sprintf(Prix,"%.2f",prix);
    sprintf(Quantite,"%d",quantite);

    // Ajout possible
    int nbLignes = ui->tableWidgetPanier->rowCount();
    nbLignes++;
    ui->tableWidgetPanier->setRowCount(nbLignes);
    ui->tableWidgetPanier->setRowHeight(nbLignes-1,10);

    QTableWidgetItem *item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(article);
    ui->tableWidgetPanier->setItem(nbLignes-1,0,item);

    item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(Prix);
    ui->tableWidgetPanier->setItem(nbLignes-1,1,item);

    item = new QTableWidgetItem;
    item->setFlags(Qt::ItemIsSelectable|Qt::ItemIsEnabled);
    item->setTextAlignment(Qt::AlignCenter);
    item->setText(Quantite);
    ui->tableWidgetPanier->setItem(nbLignes-1,2,item);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::videTablePanier()
{
    ui->tableWidgetPanier->setRowCount(0);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
int WindowClient::getIndiceArticleSelectionne()
{
    QModelIndexList liste = ui->tableWidgetPanier->selectionModel()->selectedRows();
    if (liste.size() == 0) return -1;
    QModelIndex index = liste.at(0);
    int indice = index.row();
    return indice;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions permettant d'afficher des boites de dialogue (ne pas modifier ////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::dialogueMessage(const char* titre,const char* message)
{
   QMessageBox::information(this,titre,message);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::dialogueErreur(const char* titre,const char* message)
{
   QMessageBox::critical(this,titre,message);
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
/////// CLIC SUR LA CROIX DE LA FENETRE /////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::closeEvent(QCloseEvent *event)
{
  //verification login
  if(logged)
    on_pushButtonLogout_clicked();

  //Envoi d'une requete DECONNECT au serveur
  MESSAGE m;

  m.type=1;
  m.expediteur=getpid();
  m.requete=DECONNECT;

  if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
  {
    perror("Erreur de msgsnd");
    exit(1);
  }

  exit(0);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Fonctions clics sur les boutons ////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonLogin_clicked()
{
    // Envoi d'une requete de login au serveur

    MESSAGE m;

    m.type=1;
    m.expediteur=getpid();
    m.requete=LOGIN;

    m.data1=isNouveauClientChecked();
    strcpy(m.data2, getNom());
    strcpy(m.data3, getMotDePasse());

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd CLIENT");
      exit(1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonLogout_clicked()
{
    // Envoi d'une requete CANCEL_ALL au serveur (au cas où le panier n'est pas vide)   

    MESSAGE m;

    m.type=1;
    m.expediteur=getpid();
    m.requete=CANCEL_ALL;

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }

    // Envoi d'une requete de logout au serveur

    m.requete=LOGOUT;

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }

    logged = false;
    logoutOK();
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonSuivant_clicked()
{
    // Envoi d'une requete CONSULT au serveur
    MESSAGE m;

    m.expediteur=getpid();
    m.type=1;
    m.requete=CONSULT;
    m.data1=(articleEnCours.id)+1;

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd suivant");
      exit(1);
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonPrecedent_clicked()
{
    // Envoi d'une requete CONSULT au serveur
    if(articleEnCours.id==1)
      return;

    MESSAGE m;

    m.expediteur=getpid();
    m.type=1;
    m.requete=CONSULT;
    m.data1=(articleEnCours.id)-1;

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonAcheter_clicked()
{
    // Envoi d'une requete ACHAT au serveur

    if(getQuantite()<1)
      return;
      
    MESSAGE m;

    m.type=1;
    m.requete=ACHAT;
    m.expediteur=getpid();

    m.data1=articleEnCours.id;
    sprintf(m.data2, "%d", getQuantite());

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonSupprimer_clicked()
{
    // Envoi d'une requete CANCEL au serveur

    int indArt=getIndiceArticleSelectionne();

    MESSAGE m;

    if(indArt==-1)
    {
      dialogueErreur("ERREUR", "Pas d'article selectionne");
      return;
    }

    m.type=1;
    m.requete=CANCEL;
    m.expediteur=getpid();

    m.data1=indArt;

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }



    // Mise à jour du caddie
    w->videTablePanier();
    totalCaddie = 0.0;
    w->setTotal(-1.0);

    // Envoi requete CADDIE au serveur

    m.requete=CADDIE;

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }


}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonViderPanier_clicked()
{
    // Envoi d'une requete CANCEL_ALL au serveur


    MESSAGE m;

    m.type=1;
    m.requete=CANCEL_ALL;
    m.expediteur=getpid();


    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }





    // Mise à jour du caddie
    w->videTablePanier();
    totalCaddie = 0.0;
    w->setTotal(-1.0);

    // Envoi requete CADDIE au serveur

    m.requete=CADDIE;

    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }

}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void WindowClient::on_pushButtonPayer_clicked()
{
    // Envoi d'une requete PAYER au serveur

    MESSAGE m;

    m.type=1;
    m.requete=PAYER;
    m.expediteur=getpid();


    if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
    {
      perror("Erreur de msgsnd");
      exit(1);
    }


    char tmp[100];
    sprintf(tmp,"Merci pour votre paiement de %.2f ! Votre commande sera livrée tout prochainement.",totalCaddie);
    dialogueMessage("Payer...",tmp);

    // Mise à jour du caddie
    w->videTablePanier();
    totalCaddie = 0.0;
    w->setTotal(-1.0);

}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////
///// Handlers de signaux ////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handlerSIGUSR1(int sig)
{
    MESSAGE m;
    MESSAGE reponse;
  
    if(msgrcv(idQ,&m,sizeof(MESSAGE)-sizeof(long),getpid(),IPC_NOWAIT) != -1)  // !!! a modifier en temps voulu !!!
    {
      switch(m.requete)
      {
        case LOGIN :
                    if(m.data1==1)
                    {
                      logged=true;
                      w->loginOK();
                      w->dialogueMessage("Login", m.data4);

                      numSem=int(m.data5);

                      if(semctl(idSem,numSem,SETVAL, 0) == -1)
                      {
                        perror("Erreur de semctl (1)");
                        exit(1);
                      }

                      reponse.expediteur=getpid();
                      reponse.type=1;
                      reponse.requete=CONSULT;
                      reponse.data1=1;//id premiere article

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd");
                        exit(1);
                      }
                    }
                    else
                    {
                      w->dialogueErreur("Login echoue", m.data4);
                    }

                    break;

        case CONSULT : 

                    int stock;

                    stock=atoi(m.data3);
                    articleEnCours.id=m.data1;
                    strcpy(articleEnCours.intitule, m.data2);
                    articleEnCours.stock=stock;
                    strcpy(articleEnCours.image, m.data4);
                    articleEnCours.prix=m.data5;

                    w->setArticle(m.data2, m.data5, stock, m.data4);

                    break;

        case ACHAT : 
                    char msg[100];

                    if(strcmp(m.data3, "-1")==0)
                    {
                      w->dialogueErreur("ERREUR", "Panier rempli");
                    }
                    else if(strcmp(m.data3, "0")==0)
                    {
                      w->dialogueErreur("ERREUR", "Stock insuffisant");
                    }
                    else
                    {
                      strcpy(msg, m.data3);
                      strcat(msg, " unité(s) de ");
                      strcat(msg, m.data2);
                      strcat(msg, " achetées avec succès");
                      w->dialogueMessage("ACHAT", msg);
 
                      w->videTablePanier();
                      totalCaddie=0;

                      reponse.requete=CADDIE;
                      reponse.type=1;
                      reponse.expediteur=getpid();

                      if(msgsnd(idQ,&reponse,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                      {
                        perror("Erreur de msgsnd");
                        exit(1);
                      }
                    }
                    break;

        case CADDIE : 
                      if(m.data1!=-1)
                      {
                        w->ajouteArticleTablePanier(m.data2, m.data5, atoi(m.data3));
                        totalCaddie=totalCaddie+(atoi(m.data3)*m.data5);
                        w->setTotal(totalCaddie);

                        sem_signal(numSem);
                      }
                      else
                      {
                        
                        m.type=1;
                        m.expediteur=getpid();
                        m.requete=CONSULT;

                        m.data1=articleEnCours.id;

                        if(msgsnd(idQ,&m,sizeof(MESSAGE)-sizeof(long), 0) == -1)
                        {
                          perror("Erreur de msgsnd");
                          exit(1);
                        }
                      }

                    break;  

        case TIME_OUT :

                    logged = false;

                    w->dialogueErreur("TIME OUT", "Vous avez ete deconnecté");
                    
                    w->logoutOK();
                    
                    break;

        case BUSY :

                    w->dialogueErreur("MAINTENANCE", "Serveur en maintenance, réessayez plus tard… Merci.");
                    break;

        default :
                    break;
      }
    }
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////
void handlerSIGUSR2(int sig)
{
  w->setPublicite(pShm);
}