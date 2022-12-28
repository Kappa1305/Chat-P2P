
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

int creaSocket(srvPort) {
    int ret, sd, len;
    struct sockaddr_in srv_addr;


    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    /* Creazione indirizzo del server */
    memset(&srv_addr, 0, sizeof(srv_addr)); // Pulizia 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(srvPort);
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    ret = connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));

    if (ret < 0) {
        perror("Errore in fase di connessione: \n");
        exit(-1);
    }
    return sd;
}

int commandHandshake(int sd, int command) {
    int num;
    sendNum(sd, command);
    if (!recvNum(sd)) {
        printf("handshake andato a buon fine\n");
        return 0;
    }
    else {
        printf("handshake fallito\n");
        return 1;
    }
}

void signup(srvPort) {
    int sd = creaSocket(srvPort);
    commandHandshake(sd, 1);
    char buffer[1024];
    printf("Inserire username e password\n");
    scanf("%s", buffer);
    sendMsg(sd, buffer);
    scanf("%s", buffer);
    sendMsg(sd, buffer);
}

void in(srvPort) {
    commandHandshake(sd, 2);
    char buffer[1024];
    printf("Inserire username e password\n");
    scanf("%s", buffer);
    sendMsg(sd, buffer);
    scanf("%s", buffer);
    sendMsg(sd, buffer);
}

int main(int argc, char* argv[]) {
    int ret, sd, len;
    len = 20;
    char srvPort = argv[1];
    char command[20];
    printf("porta usata %s", argc[1]);
    while (1) {
        printf("Choose operation:\n"
            "- signup [username] [password]\n"
            "- in [server port] [username] [password]\n"
            "> ");

        scanf("%s", command);
        if (!strcmp(command, "signup")) {
            signup(srvPort);
            continue;
        }

        if (!strcmp(command, "in")) {
            in(srvPort);
            continue;
        }
        // default
        printf("! Invalid operation\n");
    }


    close(sd);
}