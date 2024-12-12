#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>

#include "disk.h"
#include "pss.h"

// se crea el mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

// se crea el struct para las request de cada thread
typedef struct {
  int ready;
  pthread_cond_t w;
} Request;

//se crea una variable global que indique si el mutex esta ocupado
int busy = 0;

//se crea una variable global que indique la ultima posicion del cabezal
int posicion = 0;
 
//se crean dos colas de prioridad, una va a contener las request de posiciones
//mayores a la actual, la otra las menores

PriQueue *colaUno;
PriQueue *colaDos;

//el rol de tener los elementos mayores y menores se ira turnando a medida que se
//vacian las colas, por lo que debe haber una variable global que indique el estado

int estado = 0;

// Estado 0: colaUno contiene los mayores y colaDos los menores 
// Estado 1: colaDos contiene los mayores y colaUno los menores

//Se intercambian los roles cuando la cola que contiene los mayores se vacia.

void iniDisk(void) {
  //se inicializan ambas colas
  colaUno = makePriQueue();
  colaDos = makePriQueue();
}

void cleanDisk(void) {
  //se limpia la memoria utilizada por las colas
  destroyPriQueue(colaUno);
  destroyPriQueue(colaDos);
}

void requestDisk(int track) {
  //se cierra el mutex
  pthread_mutex_lock(&mutex);
  //si el cabezal no esta ocupado, entonces ahora lo esta
  if(!busy){
    busy=1;
  // se esta ocupado
  } else {
    //se crea la request 
    Request req = {0,PTHREAD_COND_INITIALIZER};
    // Se anade la request a su cola correspondiente en relacion a la posicion del cabezal
    if(track >= posicion){
      //se define la referencia a la cola mayor dependiendo del estado
      PriQueue* colaMayor = !estado? colaUno: colaDos;
      priPut(colaMayor,&req,track);
    }else{
      //se define la referencia a la cola menor dependiendo del estado
      PriQueue* colaMenor = !estado? colaDos : colaUno;
      priPut(colaMenor,&req,track);
    }
    //mientras la request no este lista se queda esperando
    while(!req.ready){
      pthread_cond_wait(&req.w,&mutex);
    }
  }
  //el cabezal ahora esta en track
  posicion = track;
  //se libera el mutex
  pthread_mutex_unlock(&mutex);
}

void releaseDisk() {
  //se cierra el mutex
  pthread_mutex_lock(&mutex);
  //si ambas colas estan vacias, entonces busy = 0
  if(emptyPriQueue(colaUno) && emptyPriQueue(colaDos)){
    busy = 0;
  } else {
    //dependiendo de cual sea el estado actual se elige una 
    //cola de la cual sacar la request y despertar a su thread correspondiente
    PriQueue* pCola = !estado ? colaUno : colaDos;
    //si la cola elegida esta vacia, hay que cambiar de estado
    if(emptyPriQueue(pCola)){
      estado = !estado ? 1 : 0;
      pCola = !estado ? colaUno : colaDos;
    }
    //ahora si, se atiende la request 
    Request* prequest = priGet(pCola);
    prequest->ready=1;
    pthread_cond_signal(&prequest->w);
  }
  //se abre el mutex
  pthread_mutex_unlock(&mutex);
}
