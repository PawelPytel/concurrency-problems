#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <time.h>
#define N 10
#define K 5




struct msgbuf{ //struktura opisująca dzielo
  long mtype;  // [1,K] to miejsca na polce
  char mtext[10]; //treść dzieła
};
struct orderbuf{ //struktura opisująca zamówienie
  long mtype;//[1,N] jaki proces zamawia
  int position; //[1,K] z jakiej półki zamawia
};
static struct sembuf buf;  //struktura u�ywana w operacjach na semaforze
void podnies(int semid, int semnum){ //podnoszenie semafora
  buf.sem_num = semnum;
  buf.sem_op = 1;
  buf.sem_flg = 0;
  if (semop(semid, &buf, 1) == -1){
    perror("Podnoszenie semafora");
  exit(1);
  }
}
void opusc(int semid, int semnum){ //opuszczenie semafora
  buf.sem_num = semnum;
  buf.sem_op = -1;
  buf.sem_flg = 0;
  if (semop(semid, &buf, 1) == -1){
    perror("Opuszczenie semafora");
  exit(1);
  }
}
void czekaj(int semid, int semnum){ //czekanie na wyzerowanie semafora
  buf.sem_num=semnum;
  buf.sem_op=0;
  buf.sem_flg=0;
  semop(semid, &buf, 1);
}

int main(){

  printf("start\n");
  srand(time(NULL));
  int faza; //relaks-1,czytelnia-0
  int kolejka; //kolejka odpowiadajace poszczegolnym miejscom na polce w ktorych zapisujemy jakie procesy maja odczytac dane

  struct msgbuf dzielo; //dzieło
  struct orderbuf order;// zamówienie
  int* role; //wskaażnik na fragment pamięci przechowujący informacje o rolach pełnionych przez procesy (0-czytelnik, 1-pisarz)
  int* dlugosci; //wskaźnik na tablicę z dlugosciami kolejek (ile jeszcze procesow musi przeczytac dzielo)
  char brudnopis[10]="lalala";
  char odczyt[10];
  int polka=msgget(12345,0666|IPC_CREAT); //tworzenie półki
  int procesy=shmget(54321,N*sizeof(int),IPC_CREAT|0666);//tworzenie pamięci współdzielonej dla informacji o rolach
  role=shmat(procesy,NULL,0);
  int read=shmget(11111,N*sizeof(int),IPC_CREAT|0666);//tworzenie pamięci współdzielonej dla informacji o tym, czy dany proces musi coś odczytać z czytelni
  int* do_odczytu=shmat(read,NULL,0);
  int dl_kolejki=shmget(55555,(K+3)*sizeof(int),IPC_CREAT|0666); //tworzenie pamięci współdzielonej dla ilości procesów ubiegających się o dzieło
  dlugosci=shmat(dl_kolejki,NULL,0);
  kolejka=msgget(22222,0666|IPC_CREAT);// tworzenie kolejki zamówień
  for (int i=0;i<K;i++) dlugosci[i]=0;
  dlugosci[K]=0; //początkowa ilość dzieł na półce
  dlugosci[K+1]=0; //początkowa ilość czytelników w czytelni
  dlugosci[K+2]=0; // początkowa ilość procesów w roli czytelnika
  int semid = semget(45281,5,IPC_CREAT|0600); //tworzenie semaforów kontrolujących:0- czy pisarz pisze dzieło,1-czy czytelnik wchodzi lub wychodzi z czytelni, 2-służący do synchronizacji tworzenia dodatkowych procesów,3 - służący do synchronizacji dostępu do tablicy rola i ostatniego pola tablicy długości,4 - kontroluje ilość miejsc na półce
  semctl(semid,0,SETVAL,1); // ustawienie początkowych wartości semaforów
  semctl(semid,1,SETVAL,1);
  semctl(semid,2,SETVAL,N);
  semctl(semid,3,SETVAL,1);
  semctl(semid,4,SETVAL,K);

  int my_id=0;
  faza=1;
  int zmien_role;//losowa zmiana roli
  int sleep_time; //losowy czas relaksu
  int pozycja=-1;//zapamiętuje z której pozycji książkę odczytaliśmy podczas danej wizyty
  int puste; //zmienna służąca do szuakania pustego miejsca na półce
  for (int i=1;i<N;i++){ //tworzy pozostałe N-1 procesów, początkowo każdy jest w fazie relaksu, co drugi jest pisarzem, co drugi czytelnikiem
    if (fork()==0){
      my_id=i;
      faza=1;
      opusc(semid,3);
      role[i]=i%2;
      do_odczytu[i]=0;
      if (role[i]==0) dlugosci[K+2]++;
      podnies(semid,3);
      break;
    }
  }
  opusc(semid,2); //czekanie na wszystkie procesy
  czekaj(semid,2);
  while(1){
    zmien_role=rand()%1000;
    opusc(semid,3);
    if(do_odczytu[my_id]==0 && ((role[my_id]==0 && dlugosci[K+2]>=2) || (role[my_id]==1 && dlugosci[K+2]<=N-2))){
    if(zmien_role<500){
       role[my_id]=(role[my_id]+1)%2; // zmiana roli losowa
       if(role[my_id]==0) dlugosci[K+2]++;
       else dlugosci[K+2]--;
     }
    }
    podnies(semid,3);
    faza=0; //wejscie do czytelni
      if(role[my_id]==0){// proces czytelnika
        opusc(semid,1);
        dlugosci[K+1]++; //wejscie jako czytelnik
        if(dlugosci[K+1]==1) opusc(semid,0); //zablokowanie pisarzy
        podnies(semid,1);
          msgrcv(kolejka,&order,sizeof(struct orderbuf)-sizeof(long),my_id+1,0); //szukamy czy mamy coś do przeczytania
          pozycja=order.position;
          msgrcv(polka,&dzielo,sizeof(struct msgbuf)-sizeof(long),pozycja,0);
          strcpy(odczyt,dzielo.mtext);
          msgsnd(polka,&dzielo,sizeof(struct msgbuf)-sizeof(long),0);
        opusc(semid,1);
        if (pozycja!=-1){
          dlugosci[pozycja-1]--;// zmniejszamy liczbę procesów które muszą przeczytać dzieło
          do_odczytu[my_id]--;
          printf("Przeczytałem dzieło z %d pozycji, jeszcze %d musi je przeczytać, ID: %d\n",pozycja,dlugosci[pozycja-1],my_id);
          for (int i=0;i<K+2;i++) printf("%d ",dlugosci[i]);
          printf("\n");
          for (int i=0;i<N;i++){
            printf("Rola: %d, ID: %d, odczyt: %d\n", role[i], i,do_odczytu[i]);
          }
          if (dlugosci[pozycja-1]==0){ //usuwamy dzieło i zmniejszamy liczbę dzieł na półce
              msgrcv(polka,&dzielo,sizeof(struct msgbuf)-sizeof(long),pozycja,0);
              dlugosci[K]--;
              podnies(semid,4);
              printf("zdjecie %ld pozycji\n", dzielo.mtype);
              for (int i=0;i<K+2;i++) printf("%d ",dlugosci[i]);
              printf("\n");
              for (int i=0;i<N;i++){
                printf("Rola: %d, ID: %d, odczyt: %d\n", role[i], i, do_odczytu[i]);
              }
            }
          pozycja=-1;
        }
        dlugosci[K+1]--; //wychodzimy, jezeli nikt nie zostal dajemy szanse pisarzowi
        if(dlugosci[K+1]==0) podnies(semid,0);
        podnies(semid,1);
      }

      else if (role[my_id]==1){
      //proces pisarza
        opusc(semid,4);
        opusc(semid,0);
          strcpy(dzielo.mtext,brudnopis);
        for(int i=0;i<K;i++){ //szukamy pustego miejsca na półce
          if(dlugosci[i]==0){
            puste=i+1;
            break;
          }
        }
        dzielo.mtype=puste;
        order.position=puste;
        msgsnd(polka,&dzielo,sizeof(struct msgbuf)-sizeof(long),0); //umieszczamy dzieło na półce
        dlugosci[K]++; //zwiększamy ilość dzieł na półce
        opusc(semid,3);
        for (int j=0;j<N;j++){
          if (role[j]==0){ //sprawdzamy które procesy są w danym momencie czytelnikami
            dlugosci[puste-1]++;
            do_odczytu[j]++;
            order.mtype=j+1;
            msgsnd(kolejka,&order,sizeof(struct orderbuf)-sizeof(long),0); //zapisujemy które procesy są zainteresowane dziełem
          }
        }
        podnies(semid,3);
        printf("wyslalem dzielo na pozycje: %d\n", puste);
        for (int i=0;i<K+2;i++) printf("%d ",dlugosci[i]);
        printf("\n");
        for (int i=0;i<N;i++){
          printf("Rola: %d, ID: %d, odczyt: %d\n", role[i],i,do_odczytu[i]);
        }
        podnies(semid,0);
      }

  faza=1;
  sleep_time=rand()%3+1; //faza relaksu
  sleep(sleep_time);
  }
return 0;
}
