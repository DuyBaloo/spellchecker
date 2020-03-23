/* LIBRARIES */
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <pthread.h>

/* MACROS */
#define SOCKET_DEFAULT 5000
#define DICT_DEFAULT "dictionary.txt"
#define DICT_BUF 128
#define BUFFER_MAX 5

/* GLOBAL VARS */
FILE *DICTIONARY;
FILE *LOG;
int LISTEN_PORT;

