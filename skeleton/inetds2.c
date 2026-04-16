#include <pthread.h>
#include <stdio.h>
#include <ncurses.h>
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



int inet_socket (uint16_t port, short reuse) {
  int sock;
  struct sockaddr_in name;
  
  /* Create the socket. */
  sock = socket (PF_INET, SOCK_STREAM, 0);
  if (sock < 0) {
//    perror ("socket");
    pthread_exit (NULL);
  }
  
  if (reuse) {
      int reuseAddrON = 1;
      if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuseAddrON, sizeof(reuseAddrON)) < 0) {
          perror("setsockopt(SO_REUSEADDR) failed");
    	  pthread_exit (NULL);
        }
    }

  /* Give the socket a name. */
  name.sin_family = AF_INET; 
  name.sin_port = htons (port);
  name.sin_addr.s_addr = htonl (INADDR_ANY);
  if (bind (sock, (struct sockaddr *) &name, sizeof (name)) < 0) {
    perror ("bind");
    pthread_exit (NULL);
  }
  
  return sock;
}

int get_client_id (int sock) {
  int size ;
  long clientid ;
  size = recv (sock, &clientid, sizeof (clientid), 0) ;
  fprintf (stderr, "gcid:\tReceived %ld\n", clientid) ;

  if (size == -1) return -1 ;

  return clientid ;
}



int write_client_int (int sock, long id) {
  /* DUMMY FUNCTION FOR SENDING CLIENT ID */
  int size ;
  fprintf (stderr, "wcl:\tWriting %ld\n", id) ;
  if ((size = send (sock, &id, sizeof (id), 0)) < 0) return -1 ;
  return EXIT_SUCCESS ;
}

int write_client_id (int sock, long id) {
  return write_client_int (sock, id) ;
}

int write_client_concat (int sock, char *o1, char *o2) {
  /* DUMMY IMPLEMENTATION OF CONCATENATE FUNCTION  */
  int size, bsize ;
  char *b ;

  bsize = strlen (o1) + strlen (o2)+2 ;
  b = malloc (256) ;
  sprintf (b, "%s %s", o2, o1) ; b [bsize] = 0 ;
  if ((size = send (sock, b, bsize, 0)) < 0) return -1 ;

  free (b) ;

  return EXIT_SUCCESS ;
}

char *get_client_str (int sock,  int *dsize) {
  /* DUMMY FUNCTION FOR GETTING CLIENT DATA
	Notice that FULL CLIENT DATA could come: e.g
	OPERATION^@DATA1^@DATA2^@ */
  char buffer[256] ;
  char *result = NULL;
  int isize = 0, size, i = 0 ; 
  while (((size = recv (sock, &buffer, 256, 0)) >= 0) ) {
    result = realloc (result, isize+size) ; 
    memcpy (result+isize, buffer, size) ; i++ ; 
    isize += size ; result [isize] = 0; 
    if (size != 256) break ;
  }
  if (isize < 0) return NULL ;
  fprintf (stderr, "gcs:\tGot %s\n", result) ;
  *dsize = isize ;
  /* Notice: use smarter mem allocation here ;) */
  return result ;
}

int create_client_id () {
  /* Create some unique ID. e.g. UNIX timestamp... */
  char ctsmp [12] ;  
  time_t rawtime;
  struct tm * timeinfo;
  int uuid ; 
  
  time ( &rawtime );
  timeinfo = localtime ( &rawtime );

  strftime (ctsmp, 12, "%s", timeinfo) ;

  uuid = atoi (ctsmp) ;
  /* NOTICE: YOU NEED TO STORE THIS INFORMATION AT SERVER LEVEL !*/
  return uuid ;
}

int extract_client_operation (char *data) {


  return -1 ;
}

/*
void closewin (WINDOW *wnd) {
  if (wnd != NULL) delwin (wnd) ;
  wnd = NULL ;
  refresh () ;
}
*/

void *inet_main (void *args) {
  int port = *((int *)args) ;
  int sock ;
  size_t size;
  fd_set active_fd_set, read_fd_set;
  struct sockaddr_in clientname;


  if ((sock = inet_socket (port, 1)) < 0) {
    pthread_exit (NULL);
  }
  if (listen (sock, 1) < 0) {
    pthread_exit (NULL);
  }  
  /* Initialize the set of active sockets. */
  FD_ZERO (&active_fd_set);
  FD_SET (sock, &active_fd_set);
  
  while (1) {
    int i ;
    /* Block until input arrives on one or more active sockets. */
    read_fd_set = active_fd_set;
    if (select (FD_SETSIZE, &read_fd_set, NULL, NULL, NULL) < 0) {
      pthread_exit (NULL);
    }
    
    /* Service all the sockets with input pending. */
    for (i = 0; i < FD_SETSIZE; ++i)
      if (FD_ISSET (i, &read_fd_set)) {
	if (i == sock) { /* Connection request on original socket. */
	  int new;
	  size = sizeof (clientname);
	  new = accept (sock, (struct sockaddr *) &clientname,
			(socklen_t *) &size);
	  if (new < 0) {
            pthread_exit (NULL);
	  }

	  FD_SET (new, &active_fd_set);
	}
	else { /* Data arriving on an already-connected socket. */
          int clientID ;
	  /* Protocolul simplu utilizat:
	  	Se verifica intai header-ul.
	  	Pe baza informatiei din header, se decide operatia de urmat!
			
	  */
	  msgHeaderType h = peekMsgHeader (i) ; 
 	  if ((clientID = h.clientID) < 0) {
             // Protocol error: missing client ID. Close connection
             fprintf (stderr, "There's something wrong! Negative ClientID.\t Closing connection, probably the client was terminated.\n") ;
             close (i); FD_CLR (i, &active_fd_set);
          } else {
            if (clientID == 0) {
               int newID ; 
               msgIntType m ;
               newID = create_client_id () ;
               fprintf (stderr, "\tDetected new client! New clientID: %d\n", newID ) ; 
               if (readSingleInt (i, &m)<0) {
                 // Cannot read from client. This is impossible :) Close connection!
                 close (i) ; FD_CLR (i, &active_fd_set) ;
               }
               if (writeSingleInt (i, h, newID) < 0) {
                 // Cannot write to client. Close connection!
                 close (i) ; FD_CLR (i, &active_fd_set) ;
               } 
            } else { /* Already identified. Existing client... communication continues */
              /* YOU SHOULD CHECK IF THIS IS AN EXISTING CLIENT !!! */
              int operation, dsize ;
              char *opr1, *opr2;
              int num1, num2 ;
              char *opr ;
              operation = h.opID ;
              if (operation == -1) { // Protocol error: missing or incorect operation
                 close (i); FD_CLR (i, &active_fd_set);
              }

              switch (operation) {
                 case OPR_ECHO: 
                      {
                      	 msgStringType str ;
	                 if (readSingleString (i, &str) < 0) {
	                        // Cannot write to client. Close connection!
	                        close (i) ; FD_CLR (i, &active_fd_set) ;
                         }
                 	fprintf (stderr, "An echo value was received: %s\n", str.msg) ;
                      	if (writeSingleString (i, h, str.msg) < 0) {
                 		// Cannot write to client. Close connection!
                 	  close (i) ; FD_CLR (i, &active_fd_set) ;
                 	}
                 	fprintf (stderr, "An echo value was sent back: %s\n", str.msg) ;
                        free (str.msg) ; // Need to free, once is used!
                      }    
                      break ;
                 case OPR_CONC: 
                      break ;
                 case OPR_ADD: 
                      {
                      	msgIntType m1, m2, m ;
                      	if (readMultiInt (i, &m1, &m2)<0) {
                      		// Cannot read from client. This is impossible :) Close connection!
                      	  close (i) ; FD_CLR (i, &active_fd_set) ;
                      	}
                      	// Complex implementation for negative value :D
                      	m.msg = m1.msg+m2.msg ;
                      	if (writeSingleInt (i, h, m.msg) < 0) {
                 		// Cannot write to client. Close connection!
                 	  close (i) ; FD_CLR (i, &active_fd_set) ;
                 	}
                 	fprintf (stderr, "An adder value was sent back: %d\n", m.msg) ;
                      } 
                      break ;
                 case OPR_NEG: 
                      {
                      	msgIntType m ;
                      	if (readSingleInt (i, &m)<0) {
                      		// Cannot read from client. This is impossible :) Close connection!
                      	  close (i) ; FD_CLR (i, &active_fd_set) ;
                      	}
                      	// Complex implementation for negative value :D
                      	m.msg = -m.msg ;
                      	if (writeSingleInt (i, h, m.msg) < 0) {
                 		// Cannot write to client. Close connection!
                 	  close (i) ; FD_CLR (i, &active_fd_set) ;
                 	}
                 	fprintf (stderr, "A negative value was sent back: %d\n", m.msg) ;
                      } 
                      break ;
                 case OPR_BYE: 
                 default:
                      close (i) ; FD_CLR (i, &active_fd_set) ; 
                      break ;
              } 
            }
          }
	}
      }
  }

  pthread_exit (NULL) ;
}

