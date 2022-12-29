
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utility.c"

#define MESSAGE_LEN 20 // Lunghezza del messaggio dal client

struct dev {
    int port;
    char* username;
};

struct serverStruct {
    int sd;
    int port;
};

struct dev thisDev;

struct serverStruct server;

int creaSocket() {
    int sd;
    struct sockaddr_in srv_addr;


    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("Something went wrong during socket()\n");
        exit(-1);
    }
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("Something went wrong during setsocket()\n");
        // errore non grave, posso far andare avanti
    }
    /* Creazione indirizzo del server */
    memset(&srv_addr, 0, sizeof(srv_addr)); // Pulizia 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(server.port);
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    if (connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("Something went wrong during connect: \n");
        exit(-1);
    }
    return sd;
}

void sendCommand(int sd, int command) {
    sendNum(sd, command);
}

void signup() {
    char username[1024];
    char password[1024];
    int sd, ret;

    printf("Insert [server port] [username] [password]\n");
    
    scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);

    sd = creaSocket(server.port);

    sendCommand(sd, 1);

    sendMsg(sd, username);
    sendMsg(sd, password);

    ret = recvNum(sd);
    if (ret == 0) {
        printf("registrazione effettuata con successo\n");
    }
    else
        printf("registrazione fallita\n");

    close(sd);


    close(sd);
}

void in() {
    int ret;
    int sd;
    char username[1024];
    char password[1024];
    char buffer[1024];
    printf("Insert [server port] [username] [password]\n");
    scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);

    sd = creaSocket();

    sendCommand(sd, 2);
    sendMsg(sd, username);
    sendMsg(sd, password);

    ret = recvNum(sd);
    if (ret == 0) {
        printf("login effettuato con successo\n");
    }
    else
        printf("login fallito\n");

    close(sd);

}


int main(int argc, char* argv[]) {
    int ret, len;
    char command[20];
    if (argc != 2) {
        printf("Syntax error!\nCorrect syntax is: ./dev [port]\n");
        exit(-1);
    }
    len = 20;
    thisDev.port = atoi(argv[1]);
    while (1) {
        printf("Choose operation:\n"
            "- signup [username] [password]\n"
            "- in [server port] [username] [password]\n"
            "> ");

        scanf("%s", command);
  



        if (!strcmp(command, "signup")) {
            signup();
            continue;
        }
        if (!strcmp(command, "in")) {
            in();
            continue;
        }
        // default
        printf("! Invalid operation\n");
    }

}