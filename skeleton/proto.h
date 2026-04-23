// Definitii simple pentru suportul unui protocol simplu

#define OPR_CONNECT 0  // Cere un clientID: activat cand clientID=0
#define OPR_ECHO 1     // Returneaza un echo 
#define OPR_CONC 2     // Returneaza concatenarea a doua siruri
#define OPR_NEG  3     // Returneaza opusul numarului (intreg)
#define OPR_ADD  4     // O operatie simpla de adunare (+)
#define OPR_BYE  5     // Inchide socket-ul.
#define OPR_SOLVE 6    // Rezolva un sistem liniar Ax=b prin LAPACKE 

typedef struct msgHeader {
  int msgSize ;				  // dimensiunea mesajului curent
  int clientID ;				  // clientID-ul tau
  int opID ;   				  // operatia ceruta
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
  msgIntType i ;				  
} singleIntMsgType ;
// 12 4

typedef struct multiIntMsg {
  msgHeaderType header ;
  msg2IntType i ;				  
} multiIntMsgType ;
// 12 8

typedef struct stringMsg {
//  int strSize ; 				 
  char *msg ;
} msgStringType ;


typedef struct string2Msg {
  msgStringType msg1, msg2 ;
} msg2StringType ;

typedef struct singleStringMsg {
  msgHeaderType header ;
  msgStringType msg ;				  
} singleStringType ;

typedef struct multiStringMsg {
  msgHeaderType header ;
  msg2StringType s ;				 
} multiStringType ;

msgHeaderType peekMsgHeader (int sock) 	; 
int readSingleInt (int sock, msgIntType *m)  		; // Functii  de citire/scriere pentru SingleInt
int readSingleString (int sock, msgStringType *m)  	; // Functii  de citire/scriere pentru SingleString
int writeSingleInt (int sock, msgHeaderType h, int i) ;			// Construieste mesajul si il trimite
int writeSingleString (int sock, msgHeaderType h, char *s) ;			// Construieste mesajul si il trimite

/* Trimis de client inainte de octetii matricei/vectorului */
typedef struct solveRequestHeader {
    msgHeaderType hdr;
    int32_t       n;          /* ordinul sistemului */
} solveRequestHeaderType;

/* Trimis de server inainte de octetii solutiei */
typedef struct solveResponseHeader {
    msgHeaderType hdr;
    int32_t       status;     /* 0=ok, -1=singular, -2=alocare */
    int32_t       n;          /* ordinul sistemului — necesar clientului ca sa primeasca x */
} solveResponseHeaderType;


int writeSolveRequest  (int sock, msgHeaderType h,
                        int n, const double *A, const double *b);
int readSolveRequest   (int sock, int *n, double **A, double **b);
int writeSolveResponse (int sock, msgHeaderType h,
                        int status, int n, const double *x);
int readSolveResponse  (int sock, int *status, int *n, double **x);	// Citeste raspunsul si  aloca x
