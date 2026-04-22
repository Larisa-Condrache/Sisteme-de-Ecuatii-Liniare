//gsoap ns service name: SampleServices
//gsoap ns service style: document
//gsoap ns service encoding: literal
//gsoap ns schema namespace: http://tempuri.org/ns.xsd

typedef struct concatStruct {
  long id 1;
  char * op1 1;
  char * op2 1;
} ns__concatType;

typedef struct addStruct {
  long id 1;
  long op1 1;
  long op2 1;
} ns__addType ;

typedef struct byeStruct {
  long id 1 ;
} ns__byeType ;

typedef char * ns__stringType  ;
typedef long ns__longType  ;

int ns__connect (ns__longType *connect) ; // fara cerere

int ns__echo (ns__stringType echoRequest 1, ns__stringType *echo) ;

int ns__concat (ns__concatType concatRequest 1, ns__stringType *concat) ;

int ns__adder (ns__addType adderRequest 1, ns__longType *adder) ;

int ns__bye (ns__byeType byeRequest 1, struct ns__byeResponse { } *bye); // fara raspuns
