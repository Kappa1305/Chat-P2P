
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

int main(int argc, char* argv[]) {

    int ret, sd, new_sd, len;
    struct sockaddr_in my_addr, cl_addr;
    char buffer[1024];

    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);

    /* Creazione indirizzo */
    memset(&my_addr, 0, sizeof(my_addr));
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4242);
    inet_pton(AF_INET, "127.0.0.1", &my_addr.sin_addr);

    /* Aggancio del socket all'indirizzo */
    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));

    /* Inizio dell'ascolto, coda da 10 connessioni */
    ret = listen(sd, 10);
    if (ret < 0) {
        perror("Errore in fase di bind: \n");
        exit(-1);
    }

    while (1) {

        // Dimensione dell'indirizzo del client
        len = sizeof(cl_addr);

        // Accetto nuove connessioni 
        //*** ATTENZIONE: BLOCCANTE!!! ***
        new_sd = accept(sd, (struct sockaddr*)&cl_addr, &len);

        // Creo la risposta
        strcpy(buffer, "We!");
        len = strlen(buffer);

        // Invio risposta (senza '\0' perche' strlen non lo include nella lunghezza)
        ret = send(new_sd, (void*)buffer, len, 0);

        if (ret < 0) {
            perror("Errore in fase di invio: \n");
        }

        close(new_sd);
    }
}