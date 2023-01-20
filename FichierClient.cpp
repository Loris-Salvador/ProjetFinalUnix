#include "FichierClient.h"
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <cstdio>
#include <cstring>

int estPresent(const char* nom)
{
  int fd, i=0, test=0;
  CLIENT c;

  if((fd = open("Client.dat", O_RDONLY)) == -1) 
  {
    perror("erreur d'open");
    return -1;
  }

  while(read(fd, &c, sizeof(CLIENT))==sizeof(CLIENT) && test==0)
  {
    i++;
    if(strcmp(c.nom, nom)==0)
      test=1;

  }

  if (close(fd))
  {
    perror("\nErreur de close()");
    exit(1);
  }

  if(test==1)
  {
    return i;
  }
  else
  {
    return 0;
  }
}

////////////////////////////////////////////////////////////////////////////////////
int hash(const char* motDePasse)
{
  int i=1, max, hash=0;
  char c;

  max=strlen(motDePasse);

  while(i<=max)
  {
    hash=hash+(i*int(motDePasse[i-1]));
    i++;
  }

  hash=hash%97;

  return hash;
}

////////////////////////////////////////////////////////////////////////////////////
void ajouteClient(const char* nom, const char* motDePasse)
{
  int fd;
  CLIENT c;

  if((fd = open("Client.dat", O_WRONLY | O_APPEND)) == -1) 
  {
    fd = open("Client.dat", O_CREAT | O_WRONLY | O_APPEND, 0777);
  }

  strcpy(c.nom, nom);
  c.hash=hash(motDePasse);


  write(fd, &c, sizeof(CLIENT));

  if(close(fd))
  {
    perror("\nErreur de close()");
    exit(1);
  }
}

////////////////////////////////////////////////////////////////////////////////////
int verifieMotDePasse(int pos, const char* motDePasse)
{
  int fd, mdp;
  CLIENT c;

  if((fd = open("Client.dat", O_RDONLY)) == -1) 
  {
    perror("erreur d'open");
    return -1;
  }

  lseek(fd, sizeof(CLIENT)*(pos-1), SEEK_SET);
  read(fd, &c, sizeof(CLIENT));

  if(close(fd))
  {
    perror("\nErreur de close()");
    exit(1);
  }

  mdp=hash(motDePasse);

  if(mdp==c.hash)
  {
    return 1;
  }

  return 0;  
}

////////////////////////////////////////////////////////////////////////////////////
int listeClients(CLIENT *vecteur) // le vecteur doit etre suffisamment grand
{
  int fd, i=0;
  CLIENT c;

  if((fd = open("Client.dat", O_RDONLY)) == -1) 
  {
    // printf("Le fichier n'existe pas");
    return -1;
  }

  lseek(fd, 0, SEEK_SET);

  while(read(fd, &c, sizeof(CLIENT))==sizeof(CLIENT))
  {
    *vecteur=c;
    i++;
    vecteur++;
  }

  if (close(fd))
  {
    perror("\nErreur de close()");
    exit(1);
  }

  return i;
}
