// Simple defines for supporting a plain protocol

#define OPR_CONNECT 0  // Asks for a clientID: activated when clientID=0, clientID is unknown, or when opID=0
#define OPR_ECHO 1     // Return an echo (which is a concatenation :D)
#define OPR_CONC 2     // Return a concatenation of two strings
#define OPR_NEG  3     // Return the negate number (integer)
#define OPR_ADD  4     // A simple ADD (+) operation
#define OPR_BYE  5     // Just close the socket.
#define OPR_SOLVE 6    // Solve a linear system Ax=b via LAPACKE (parallel fork)

typedef struct msgHeader {
  int msgSize ;				  // the size of current message
  int clientID ;				  // your clientID
  int opID ;   				  // the requested operation.
} msgHeaderType ;
// 4 4 4
typedef struct int2Msg {
  int msg1, msg2 ;
} msg2IntType; 
// 4 4 
typedef struct intMsg {
  int msg ;
} msgIntType ;
// 4

typedef struct singleIntMsg {
  msgHeaderType header ;
  msgIntType i ;				  // i.msg
} singleIntMsgType ;
// 12 4

typedef struct multiIntMsg {
  msgHeaderType header ;
  msg2IntType i ;				  // i.msg1, i.msg2
} multiIntMsgType ;
// 12 8

typedef struct stringMsg {
//  int strSize ; 				  // For a string it is required to send its length!
  char *msg ;
} msgStringType ;


typedef struct string2Msg {
  msgStringType msg1, msg2 ;
} msg2StringType ;

typedef struct singleStringMsg {
  msgHeaderType header ;
  msgStringType msg ;				  // msg.strMsg 
} singleStringType ;

typedef struct multiStringMsg {
  msgHeaderType header ;
  msg2StringType s ;				  // s.msg1.strMsg, s.msg2.strMsg
} multiStringType ;

msgHeaderType peekMsgHeader (int sock) 	; // Use this function to 'peek' into messge structure. Take a look, it doesn't heart :)
int readSingleInt (int sock, msgIntType *m)  		; // Simple read/write facilities for SingleInt
int readMultiInt (int sock, msgIntType *m1, msgIntType *m2)  		; // Simple read/write facilities for MultiInt
int readSingleString (int sock, msgStringType *m)  	; // Simple read/write facilities for singleString
int writeSingleInt (int sock, msgHeaderType h, int i) ;			// Build the message and send it!
int writeMultiInt (int sock, msgHeaderType h, int i1, int i2) ;		// Build the message and send it!
int writeSingleString (int sock, msgHeaderType h, char *s) ;			// Build the message and send it!
int writeMultiString (int sock, msgHeaderType h, char *s1, char *s2) ;

/* ------------------------------------------------------------------ */
/*  OPR_SOLVE — Linear system Ax=b                                    */
/*                                                                     */
/*  Wire format (client → server):                                     */
/*    1. msgHeaderType  (opID = OPR_SOLVE)                             */
/*    2. int32_t  n     — matrix order (n×n)                           */
/*    3. double[n*n]    — matrix A, row-major, native byte order       */
/*    4. double[n]      — vector b                                     */
/*                                                                     */
/*  Wire format (server → client):                                     */
/*    1. msgHeaderType  (opID = OPR_SOLVE)                             */
/*    2. int32_t  status — 0 = ok, -1 = singular, -2 = alloc error    */
/*    3. double[n]      — solution vector x  (only when status == 0)   */
/* ------------------------------------------------------------------ */

/* Sent by client before the raw matrix/vector bytes */
typedef struct solveRequestHeader {
    msgHeaderType hdr;
    int32_t       n;          /* system order */
} solveRequestHeaderType;

/* Sent by server before the solution bytes */
typedef struct solveResponseHeader {
    msgHeaderType hdr;
    int32_t       status;     /* 0=ok, -1=singular, -2=alloc */
} solveResponseHeaderType;

/* proto.c helpers for OPR_SOLVE */
int writeSolveRequest  (int sock, msgHeaderType h,
                        int n, const double *A, const double *b);
int readSolveRequest   (int sock, int *n, double **A, double **b);
int writeSolveResponse (int sock, msgHeaderType h,
                        int status, int n, const double *x);
int readSolveResponse  (int sock, int *status, int *n, double **x);	// Build the message and send it!

