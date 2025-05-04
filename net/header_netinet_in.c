#include <netinet/in.h>

unsigned long int htonl(unsigned long int hostlong);
unsigned short int htons(unsigned short int hostshort);
unsigned long int ntohl(unsigned long int netlong);
unsigned short int ntohs(unsigned short int netshort);

struct sockaddr_in;
struct sockaddr_in6;
socklen_t;

IN_ADDR_ANY;
IN6ADDR_ANY;
