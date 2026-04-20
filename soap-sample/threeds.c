#include <stddef.h>
#include <errno.h>
#include <pthread.h>
//#include <ncurses.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>


void *unix_main (void *args) ;
void *inet_main (void *args) ;
void *soap_main (void *args) ;

<<<<<<< HEAD
=======
// WINDOW *mainwnd ;
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619

#define SOAPPORT   18082

pthread_mutex_t curmtx = PTHREAD_MUTEX_INITIALIZER ;
int main () {
  int iport, sport ;

<<<<<<< HEAD
  pthread_t unixthr, /* Thread UNIX: componenta server UNIX */
	inetthr,     /* Thread INET: componenta server INET */
	soapthr ;     /* Thread SOAP: componenta server SOAP */
//	workerthr ;  /* Thread Worker: foloseste-l pentru sarcini WORK (diverse) */
=======
  pthread_t unixthr, /* UNIX Thread: the UNIX server component */
	inetthr,     /* INET Thread: the INET server component */ 
	soapthr ;     /* SOAP Thread: the SOAP server component */
//	workerthr ;  /* The Worker Thread: use it for WORK tasks (various) */
>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619

/*
  mainwnd = initscr () ;
  noecho () ;
  cbreak () ;
  if (has_colors()) start_color () ;
  init_pair (1, COLOR_RED, COLOR_BLACK) ;
//  refresh () ;
*/
  sport = SOAPPORT ;
  pthread_create (&soapthr, NULL, soap_main, &sport) ;
/*
  pthread_create (&workerthr, NULL, work_main, NULL) ;
    Implementarea firului de lucru:
	acest fir va rezolva sarcinile de lucru uzuale, bazate pe cereri INET/SOAP	
	firul de lucru proceseaza cererile din coada sau este suspendat (vezi mai jos)
	ar putea utiliza mecanismul 'condition variable': orice cerere noua este semnalata catre firul de lucru 
		prin acest mecanism.
 */

  pthread_join (soapthr, NULL) ;

/*
  getch () ;
  endwin () ;
  */
  return 0 ;
}
<<<<<<< HEAD
=======

>>>>>>> 5d2297c01d7b8794f53fc1a2ffcc3bc1af02a619
