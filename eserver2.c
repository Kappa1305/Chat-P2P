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
    int ret, sd, new_sd, len;
    struct sockaddr_in my_addr, cl_addr;
    char buffer[1024];
    printf("we");
    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    /* Creazione indirizzo di bind */
    memset(&my_addr, 0, sizeof(my_addr)); // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4242);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    ret = listen(sd, 10);
    if (ret < 0) {
        perror("Errore in fase di bind: \n");
        exit(-1);
    }

    while (1) {

        len = sizeof(cl_addr);

        // Accetto nuove connessioni
        new_sd = accept(sd, (struct sockaddr*)&cl_addr, &len);

        while (1) {

            // Attendo risposta
            len = MESSAGE_LEN;
            ret = recv(new_sd, (void*)buffer, len, 0);

            if (ret < 0) {
                perror("Errore in fase di ricezione: \n");
                continue;
            }

            // Posso fare strmcp perché invio anche il fine stringa '\0'
            // Invio sempre sul socket 20 byte!
            if (strcmp(buffer, "bye") == 0) {
                close(new_sd);
                break;
            }

            // Invio risposta
            ret = send(new_sd, (void*)buffer, len, 0);

            if (ret < 0) {
                perror("Errore in fase di invio: \n");
            }
        }
    }
}