#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "proto.h"

#define PORT            18081
#define SERVERHOST      "127.0.0.1" 
#define MSGLEN	128

  void init_sockaddr (struct sockaddr_in *name,
  const char *hostname,
  uint16_t port)
  {
  struct hostent *hostinfo;
  
  name->sin_family = AF_INET;
  name->sin_port = htons (port);
  hostinfo = gethostbyname (hostname);
  if (hostinfo == NULL)
  {
  fprintf (stderr, "Unknown host %s.\n", hostname);
  exit (EXIT_FAILURE);
  }
  name->sin_addr = *(struct in_addr *) hostinfo->h_addr;
  }

void write_server (int fd, char *TMESSAGE) {
  int nbytes;
  
  nbytes = send (fd, TMESSAGE, strlen (TMESSAGE) + 1, 0);
  if (nbytes < 0) {
    perror ("write");
    exit (EXIT_FAILURE);
  }
}


int main (void) {
  int sock; 
  struct sockaddr_in servername;

  
  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
    perror ("socket (client)");
    exit (EXIT_FAILURE);
  }
  
  /* Connect to the server. */
  init_sockaddr (&servername, SERVERHOST, PORT);
  if (0 > connect (sock,
		   (struct sockaddr *) &servername,
		   sizeof (servername))) {
    perror ("connect (client)");
    exit (EXIT_FAILURE);
  }
  
/* 0. Send a connect request!. */
  msgHeaderType h ;
  msgIntType m ;
  msgStringType str ;
  int clientID = 0 ;
  
  h.clientID = 0 ;
  h.opID = 0 ;
  writeSingleInt (sock, h, 0) ; // Just for tests, ignore the response!
  readSingleInt (sock,  &m) ;   // Just for tests, ignore the response!
  clientID = m.msg ;
  fprintf (stderr, "Got a clientID: %d\n", clientID) ;
/* 1. Now do some stupid math: negative of a number. */
  h.clientID = clientID ;
  h.opID = OPR_NEG ;
  fprintf (stderr, "1. Do some OPR_NEG calls...\n") ;
  for (int i =0; i < 20; i++) {
    writeSingleInt (sock, h, -100+i*15) ; // Just for tests, ignore the response!
    readSingleInt (sock,  &m) ;   // Just for tests, ignore the response!
    fprintf (stderr, "Got the negative (hopefully) version of %d ==> %d\n", -100+i*15, m.msg) ;
  }
/* 2. More stupid math: add two numbers :) */
  h.clientID = clientID ;
  h.opID = OPR_ADD ;
  fprintf (stderr, "2. Do some OPR_ADD calls...\n") ;
  for (int i =0; i < 120; i++) {
    writeMultiInt (sock, h, -100+i*15, 100-i*22) ; // Just for tests, ignore the response!
    readSingleInt (sock,  &m) ;   // Just for tests, ignore the response!
    fprintf (stderr, "Got the sum of %d %d ==> %d\n", -100+i*15, 100-i*22, m.msg) ;
  }
/* 3. ECHO */
  h.clientID = clientID ;
  h.opID = OPR_ECHO ;
  fprintf (stderr, "3. Do some OPR_ECHO calls...\n") ;
  char *outgoing = "This is a really long test message3" ;
  writeSingleString (sock, h, outgoing) ; // Just for tests, ignore the response!
  readSingleString (sock,  &str) ;   // Just for tests, ignore the response!
  fprintf (stderr, "Got the echo of %s  ==> %s\n", outgoing, str.msg) ;
  free (str.msg) ; // Need to free, once it's used!
/* 5. BYE */
  h.clientID = clientID ;
  h.opID = OPR_BYE ;
  fprintf (stderr, "5. An explicit BYE operation...\n") ;
  writeSingleInt (sock, h, 0) ; // Just for tests, ignore the response!

/* NO RESPONSE WAS EXPECTED */

  close (sock);
  exit (EXIT_SUCCESS);
}
