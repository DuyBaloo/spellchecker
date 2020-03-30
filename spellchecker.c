#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>

#define DEFAULT_PORT 5000
#define DEFAULT_DICTIONARY "dictionary.txt"
#define DICT_BUF 128
#define BUFFER_MAX 5

FILE *dictionary;
FILE *log;
int listen_port;
int client[BUFFER_MAX];
char *logs[BUFFER_MAX];

typedef struct server{
    int client_num, log_num, log_read, log_write, client_read, client_write;
    pthread_mutex_t client_mutex, log_mutex;
    pthread_cond_t client_notempty, client_notfull, log_notemty, log_notfull;
}server;
/*****************************************************************************************/

int open_listenfd(int port)
{
    //mainly get this from Computer Systems, and tutorial link provided in slide
    int listenfd, optval = 1;
    struct sockaddr_in servaddr;

    /* Create a socket descriptor */
    if ((listenfd = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        return -1;
    }

    if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR,
                   (const void *)&optval , sizeof(int)) < 0){
        return -1;
    }

    memset(&servaddr, 0, sizeof(struct sockaddr_in));
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons((unsigned short)port);
    if (bind(listenfd, (struct sockaddr*)&servaddr, sizeof(servaddr)) < 0){
        return -1;
    }

    if (listen(listenfd, 20) < 0){
        return -1;
    }

    return listenfd;
}

int lookup(char *word) 
{
    //function to get word input and compare
    int match = 0; //check if the word is valid or not

    //store the input to compare
    char buf[DICT_BUF];

    //get rid of newline since words in file already has newlines
    size_t len = strlen(word);
    word[len - 2] = '\n';
    word[len - 1] = '\0';

    while(fgets(buf, DICT_BUF, dictionary) != NULL) {
        //printf("%s", buf);

        //keep getting new word until it matches
        if(strcmp(buf, word) != 0) 
        {
            continue;
        }
        else
        {
            //printf("%s", word);
            rewind(dictionary);
            return ++match;
        }
        
    }
    rewind(dictionary);
    return match;
}

//main with argc, argv[]
int main(int argc, char *argv[]) 
{
    //if unable to open file, 
    if (argc == 1) 
    {
        if (!(dictionary = fopen(DEFAULT_DICTIONARY, "r"))) 
        {
            perror("Dictionary");
            exit(EXIT_FAILURE);
        }
        listen_port = DEFAULT_PORT;
    }

/*initiate server
get words from file
get input from user
lock the lock
do a lookup
return result
unlock the lock