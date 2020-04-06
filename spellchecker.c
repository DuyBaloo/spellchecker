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
#define DICT_BUFFER 128
#define BUFFER 5

FILE *dictionary;
FILE *log;
int listen_port;
int client[BUFFER];
char *logs[BUFFER];

typedef struct server{
    int client_num, log_num, log_read, log_write, client_read, client_write;
    pthread_mutex_t client_mutex, log_mutex;
    pthread_cond_t client_notempty, client_notfull, log_notemty, log_notfull;
}server;
/*****************************************************************************************/

int create_listenfd(int port)
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

char *remove_log(server *serv) 
{
    //function to remove the log 
    char *res = logs[serv->log_read];

    //created with the help of Prof. Fiore
    //clear current index
    logs[serv->log_read] = (char *) calloc(1, sizeof(char *));

    //pointer increment, if reach the end, wrap around to the start
    serv->log_read = (++serv->log_read) % BUFFER; 
    --serv->log_num; //decrease number of logs

    return res;
}

int remove_client(server *serv) {
    int socket = clients[serv->client_read];

    //clear index
    clients[serv->client_read] = 0;

    //if reaches the end, wrap to the beginning
    serv->client_read = (++serv->client_read) % BUFFER_MAX;

    //decrease # of clients
    --serv->client_count;

    return socket;
}

void insert_log(server *serv, char *word, int spell_check) {
    //queue of words
    char string[DICT_BUFFER];

    clients[serv->client_read] = (int) calloc(1, sizeof(int));

    //remove '\n' at the end
    size_t len = strlen(word);
    word[len - 1] = '\0';

    //variable to hold result of spellcheck
    char *res;

    //if the result matched with dictionary, return 1 to set res to OK, else res is MISPELLED
    if(spell_check)
    {
        res = "OK";
    }
    else
    {
        res = "MISPELLED";
    }
    
    //formatted as "WORD" is OK/MISPELLED
    strcpy(string, word);
    strcat(string, " is ");
    strcat(string, res);

    //put string to the queue
    strcpy(logs[serv->log_write], string);

    //move the pointer, if reaches the end, wrap around to beginning
    serv->log_write = (++serv->log_write) % BUFFER;

    //Increase number of logs
    ++serv->log_count;
}

void *log_worker(void* args)
{
    server *serv = args;

    while(1) 
    {
        //lock the lock
        pthread_mutex_lock(&serv->log_mutex);

        //constantly checks, if log is empty -> unlock the log mutex
        //with the help of 
        //https://stackoverflow.com/questions/16522858/understanding-of-pthread-cond-wait-and-pthread-cond-signal/16524148
        while (serv->log_read == serv->log_write && serv->log_num == 0) {
            pthread_cond_wait(&serv->log_notempty, &serv->log_mutex);
        }

        //remove a log
        char *res = remove_log(serv);

        fprintf(log, "%s\n", res);

        fflush(log);
        //signal telling that log is not full
        pthread_cond_signal(&serv->log_notfull);

        pthread_mutex_unlock(&serv->log_mutex);
    }
}

int lookup(char *word) 
{
    //function to get word input and compare
    int match = 0; //check if the word is valid or not

    //store the input to compare
    char buf[DICT_BUFFER];

    //get rid of newline since words in file already has newlines
    size_t len = strlen(word);
    word[len - 2] = '\n';
    word[len - 1] = '\0';

    while(fgets(buf, DICT_BUFFER, dictionary) != NULL) {
        //printf("%s", buf);

        //keep getting new word until it matches, if not returns 0
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
    char *string;

    //if unable to open file, set file to be the default, then set port to default
    if (argc == 1) 
    {
        if (!(dictionary = fopen(DEFAULT_DICTIONARY, "r"))) 
        {
            perror("Dictionary");
            exit(0);
        }
        listen_port = DEFAULT_PORT;
    }
    else if(argc == 2)
    {
        //take the input, try to set the argv[1] to be the socket port, if not successful,
        //set argv[1] to be the dictionary file and set socket port to be default, 
        //if all fails, prints error
        if(!(listen_port = (int) strtol(argv[1], &string, 10)))
        {
            if(!(dictionary = fopen(argv[1], "r")))
            {
                perror("Fail to open dictionary file.");
                exit(0);
            }
            listen_port = DEFAULT_PORT;
        }
    }
    else
    {
        //handle errors when input wrong argv[]
        //try to set argv[1] to be the file
        if(!(dictionary = fopen(argv[1], "r")))
        {
            perror("Fail to open dictionary file.");
            exit(0);
        }

        //check to see if argv[2] is socket value
        if(!(listen_port = (int) strtol(argv[2], &string, 10)))
        {
            {
                perror("Socket issue.");
                exit(0);
            }
        }
    }

    int socket;
    if((socket = create_listenfd(listen_port)) < 0)
    {
        perror("Error creating socket.");
        exit(0);
    }
    
}    
/*initiate server
get words from file
get input from user
lock the lock
do a lookup
return result
unlock the lock