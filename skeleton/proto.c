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

#define DEBUG 
// Implementation of msgHeaderType peekMsgHeader (int sock) 	

msgHeaderType peekMsgHeader (int sock) { 	 // Use this function to 'peek' into message structure. Take a look, it doesn't heart :)
	size_t nb ; 
	msgHeaderType h ;
	h.msgSize = htonl(sizeof (h)) ;
	nb = recv (sock, &h, sizeof(h), MSG_PEEK | MSG_WAITALL) ;
	// Mandatory conversions!
	h.msgSize  = ntohl (h.msgSize) ;
	h.clientID = ntohl (h.clientID) ;
	h.opID = ntohl (h.opID) ;

	// End of mandatory conversions!
	if (nb == -1) {
		h.opID = h.clientID = -1 ; // Something weird happened!
	}
	if (nb == 0) {
		h.opID = h.clientID = OPR_BYE ; // Connection closed for some reason. Just close it!
	}
#ifdef DEBUG
	fprintf (stderr, "\tReceived msgHeader: %d %d, %d (%ld)\n", h.msgSize, h.clientID, h.opID, nb) ;
#endif
	return h ;
}

int readSingleInt (int sock, msgIntType *m) {  		 // Simple read/write facilities for SingleInt
	size_t nb ; 
	singleIntMsgType s ;
	nb = recv (sock, &s, sizeof(s), MSG_WAITALL) ;	
	if (nb <= 0) {
		m->msg = -1 ;
		return -1  ;
	}
	m->msg = ntohl(s.i.msg) ; 
	return nb ;
}

int writeSingleInt (int sock, msgHeaderType h, int i) {			// Build the message and send it!
  singleIntMsgType s ;
  s.header.clientID = htonl(h.clientID) ;
  s.header.opID = htonl(h.opID) ;
  s.i.msg = htonl(i) ;
  s.header.msgSize = htonl(sizeof(s)) ;
  size_t nb ;
  nb = send (sock, &s, sizeof(s), 0) ;
  if (nb == -1) {
    // Something weird happened! Report and close
    return -1 ;
  }
  if (nb == 0) {
    // Cannot send! Connection close, Just report and close connection!
    return -1 ;
  }
  return nb ;
}

int readMultiInt (int sock, msgIntType *m1, msgIntType *m2) {  		 // Simple read/write facilities for SingleInt
	size_t nb ; 
	multiIntMsgType s ;
	nb = recv (sock, &s, sizeof(s), MSG_WAITALL) ;	
	if (nb <= 0) {
		m1->msg = m2->msg = -1 ;
		return -1  ;
	}
	m1->msg = ntohl(s.i.msg1) ; 
	m2->msg = ntohl(s.i.msg2) ; 
	return nb ;
}

int writeMultiInt (int sock, msgHeaderType h, int i1, int i2) {		// Build the message and send it!
  multiIntMsgType s ;
  s.header.clientID = htonl(h.clientID) ;
  s.header.opID = htonl(h.opID) ;
  s.i.msg1 = htonl(i1) ;
  s.i.msg2 = htonl(i2) ;
  s.header.msgSize = htonl(sizeof(s)) ;
  size_t nb ;
  nb = send (sock, &s, sizeof(s), 0) ;
  if (nb == -1) {
    // Something weird happened! Report and close
    return -1 ;
  }
  if (nb == 0) {
    // Cannot send! Connection close, Just report and close connection!
    return -1 ;
  }
  return nb ;
}

int readSingleString (int sock,  msgStringType *str) {  		 // Simple read/write facilities for SingleInt
/* REDO readSingleSting as follows: 
   Receive a singleIntFirst (with the size of your string).
   Receive the real string next. No Padding!
   */
	size_t nb ; 
	msgIntType m ;
	nb = readSingleInt (sock, &m) ;	 // Skip the header....
        fprintf (stderr, "The string size was received: %d\n", m.msg) ;
	str->msg = (char *)malloc (m.msg+1) ;
	nb = recv (sock, str->msg, m.msg, MSG_WAITALL) ;
	fprintf (stderr, "\tReceived stream is {%ld}\n", nb) ;
	
	str->msg[m.msg]='\0';
	fprintf (stderr, "\tReceived message is {%s}\n", str->msg) ;
	return nb ;
}

int writeSingleString (int sock, msgHeaderType h, char *str) {
/* REDO writeSingleString as follows:
  Send a SingleInteger first. The sent value is the string.
  Send the real string next. No Padding!
 */
 
  size_t nb ;
  int strSize = strlen(str) ;
  nb = writeSingleInt (sock, h, strSize) ;
  if (nb == -1) {
    // Something weird happened! Report and close
    return -1 ;
  }
  if (nb == 0) {
    // Cannot send! Connection close, Just report and close connection!
    return -1 ;
  }

  fprintf (stderr, "\tSent size notification [%d]\n", strSize) ;
  nb = write (2, str, strSize) ;
  nb = send (sock, str, strSize, 0) ;
  fprintf (stderr, "|\t[%ld/%ld//%ld]\n", nb, sizeof(singleStringType), sizeof(msgStringType)) ;
  return nb ;
}

