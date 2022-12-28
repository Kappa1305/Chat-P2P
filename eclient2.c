
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#define MESSAGE_LEN 20 // Lunghezza del messaggio dal client

int main(int argc, char* argv[]) {
    int ret, sd, len;
    struct sockaddr_in srv_addr;
    char buffer[1024];

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

    len = MESSAGE_LEN;

    while (1) {

        // Attendo input da tastiera
        scanf("%s", buffer);

        // Invio al server
        ret = send(sd, (void*)buffer, len, 0);

        if (ret < 0) {
            perror("Errore in fase di invio: \n");
            exit(-1);
        }

        // Attendo risposta
        ret = recv(sd, (void*)buffer, len, 0);

        if (ret < 0) {
            perror("Errore in fase di ricezione: \n");
            exit(-1);
        }

        printf("%s\n", buffer);

        if (strcmp(buffer, "bye\0") == 0)
            break;
    }

    close(sd);

}





