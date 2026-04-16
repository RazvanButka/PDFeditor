#include <pthread.h>

#include <ncurses.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include "SampleServices.nsmap"
#include "soapH.h"

/*
extern WINDOW * mainwnd ;
extern pthread_mutex_t curmtx ;

WINDOW *soapwnd; 
*/

/* Reuse this function. Previously from inetds.c */

long create_client_id () ;
void closewin (WINDOW *wnd) ;

void *soap_main (void *args) {
  struct soap soap ;
  int msd, csd, port, reuseAddrON = 1 ;
  port = *(int *)(args);
  soap_init (&soap) ;
  
  soap.bind_flags=SO_REUSEADDR ;
  msd = soap_bind (&soap, "127.0.0.1", port, 100) ;
  if (!soap_valid_socket(msd)) {
    soap_print_fault (&soap, stderr) ; 
    pthread_exit (NULL) ;
  } else {
    setsockopt(msd, SOL_SOCKET, SO_REUSEADDR, &reuseAddrON, sizeof(reuseAddrON)) ; 
  }

  for (;;) {
    csd = soap_accept (&soap) ;
    if (csd < 0) { soap_print_fault (&soap, stderr); break ; }
//    soapwnd = newwin (16, 70, 5, 5) ;
    if (soap_serve (&soap) != SOAP_OK) soap_print_fault (&soap, stderr) ;
     soap_destroy (&soap) ;
    soap_end (&soap) ;
  }
  soap_done (&soap) ;
  pthread_exit (NULL) ;
}

char *do_client_concat (struct soap *soap, char *o1, char *o2) {
  int bsize ;
  char *b ; 
  bsize = strlen (o1) + strlen (o2)+2 ;
  b = soap_malloc (soap, bsize) ;
  sprintf (b, "%s %s", o2, o1) ; b [bsize] = 0 ;



  return b ;

}
 
int ns__bye (struct soap*s, struct byeStruct rq, struct ns__byeResponse *rsp) {
  /* Do nothing for this sample. 
     In "Real" application clear structures associated with your ID */
  return SOAP_OK ;
}

int ns__connect(struct soap*s, long *rsp) {
  long clientid = create_client_id() ;
  *rsp = clientid ;
  
  return SOAP_OK ;
}

int ns__echo(struct soap*s, char *rq, char **rsp) {
  char *rsp1 ; 
  rsp1 = do_client_concat (s, ":: echo ::", rq) ;
  *rsp = rsp1 ;
  return SOAP_OK ;
}

int ns__concat(struct soap*s, struct concatStruct rq, char **rsp) {
  char *rsp1;
  rsp1 = do_client_concat (s, rq.op1, rq.op2) ;
  *rsp = rsp1 ; 
  return SOAP_OK ;
}

int ns__adder(struct soap*s, struct addStruct rq, long *rsp) {
  long rsp1 ;
  rsp1 = rq.op1 + rq.op2; 
  *rsp = rsp1 ;

  return SOAP_OK ;
}



