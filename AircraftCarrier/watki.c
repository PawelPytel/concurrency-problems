#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <time.h>
#define N 6
#define K 3

int liczba_samolotow=0;
int oczekujace=0; //liczba samolotów oczekujących na lądowanie
int na_pasie=0; //liczba samolotów korzystających z pasa, nie może przekroczyć 1
pthread_mutex_t pas=PTHREAD_MUTEX_INITIALIZER; //mutex synchronizujący dostęp do pasa
pthread_mutex_t licz=PTHREAD_MUTEX_INITIALIZER; //mutex synchronizujący dostęp do liczby samolotów na pokładzie oraz oczekujących na lądowanie
pthread_cond_t duzo=PTHREAD_COND_INITIALIZER; // zmienna warunkowa blokująca możliwość lądowania w przypadku braku miejsca na pokładzie
pthread_cond_t malo=PTHREAD_COND_INITIALIZER; //zmienna warunkowa dająca priorytet samolotom lądującym
void* samolot(void* arg){
  int odpoczynek; //czas odpoczynku
  pthread_mutex_lock(&licz);
  oczekujace++;
  pthread_mutex_unlock(&licz);
  pthread_mutex_lock(&pas); //oczekiwanie na lądowanie
  if (liczba_samolotow==N) pthread_cond_wait(&duzo,&pas);
  na_pasie++;
  sleep(2); //lądowanie
  printf("landing\n");
  pthread_mutex_lock(&licz);
  oczekujace--;
  liczba_samolotow++;
  pthread_mutex_unlock(&licz);
  na_pasie--;
  pthread_mutex_unlock(&pas);
  pthread_mutex_lock(&licz);
  if(liczba_samolotow>=K || oczekujace==0) pthread_cond_signal(&malo);
  pthread_mutex_unlock(&licz);
  odpoczynek=2;
  sleep(odpoczynek); //pobyt na lotniskowcu
  pthread_mutex_lock(&pas);
  pthread_mutex_lock(&licz);
  if(liczba_samolotow<K && oczekujace>0) pthread_cond_wait(&malo,&pas);
  pthread_mutex_unlock(&licz);
  na_pasie++;
  sleep(2); //startowanie
  printf("taking off\n");
  pthread_mutex_lock(&licz);
  liczba_samolotow--;
  pthread_mutex_unlock(&licz);
  na_pasie--;
  pthread_mutex_unlock(&pas);
  pthread_mutex_lock(&licz);
  if(liczba_samolotow<N) pthread_cond_signal(&duzo);
  pthread_mutex_unlock(&licz);
  return NULL;
}
void* control(void* arg){ //funkcja kontrolująca czy nie było wypadku
  while(1){
    if(na_pasie>1) printf("WYPADEK!!!\n");
  }
}
int main(){
  srand(time(NULL));
  pthread_t samoloty[10];
  pthread_t kontrola;
  pthread_create(&kontrola,NULL,control,NULL); //uruchomienie funkcji kontrolnej
    for (int i=0;i<10;i++){
      pthread_create(&(samoloty[i]),NULL,samolot,NULL);
    }
  while(1){
    printf("liczba samolotow: %d, na pasie %d\n",liczba_samolotow, na_pasie);
    sleep(1);
  }
  return 0;
}
