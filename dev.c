
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

int creaSocket() {
    int ret, sd, len;
    struct sockaddr_in srv_addr;


    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    /* Creazione indirizzo del server */
    memset(&srv_addr, 0, sizeof(srv_addr)); // Pulizia 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(4242);
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    ret = connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr));

    if (ret < 0) {
        perror("Errore in fase di connessione: \n");
        exit(-1);
    }
    return sd;
}

void signup() {
    int sd = creaSocket();
    char buffer[1024];

    scanf("%s", buffer);
    sendMsg(sd, buffer);
    scanf("%s", buffer);
    sendMsg(sd, buffer);
}

void in(){

}

int main(int argc, char* argv[]) {
    int ret, sd, len;
    len = 20;
    char command[20];
    while (1) {
        printf("Choose operation:\n"
            "- signup [username] [password]\n"
            "- in [server port] [username] [password]\n"
            "> ");

        scanf("%s", command);
        if (!strcmp(command, "signup")) {
            signup();
            break;
        }

        if (!strcmp(command, "in")) {
            in();
            break;
        }
        // default
        printf("! Invalid operation\n");
    }

    len = MESSAGE_LEN;

    while (1) {

        // Attendo input da tastiera
        // Attendo risposta
        //ret = recv(sd, (void*)buffer, len, 0);

        if (ret < 0) {
            perror("Errore in fase di ricezione: \n");
            exit(-1);
        }


    }

    close(sd);

}





