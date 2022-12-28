
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "utility.c"

#define MESSAGE_LEN 20 // Lunghezza del messaggio dal client

struct device {

};

void signupInsert(char username[1024], char password[1024]) {
    FILE* fptr;
    fptr = fopen("autenticazione.txt", "a");
    fprintf(fptr, "%s ", username);
    fprintf(fptr, "%s\n", password);
    fclose(fptr);
}

int signupControl(char username[1024]) {
    FILE* fptr;
    int c = 0;
    char usernamePresente[1024];
    fptr = fopen("autenticazione.txt", "r");
    while (fscanf(fptr, "%s", usernamePresente) != EOF) {
        if (c == 1) { // mi assicura di star controllando l'username e non la pswx
            c = 0;
            continue;
        }
        c = 1;
        if (!strcmp(username, usernamePresente))
            return 1;

    }
    fclose(fptr);
    return 0;
}

int loginCheck(char username[1024], char password[1024]) {
    FILE* fptr;
    int c = 0;
    char usernamePresente[1024];
    char passwordPresente[1024];
    fptr = fopen("autenticazione.txt", "r");
    while (fscanf(fptr, "%s", usernamePresente) != EOF) {
        if (c == 1) { // mi assicura di star controllando l'username e non la pswx
            c = 0;
            continue;
        }
        c = 1;
        if (!strcmp(username, usernamePresente)) {
            fscanf(fptr, "%s", passwordPresente);
            if (!strcmp(password, passwordPresente)) {
                printf("login eseguito con successo\n");
                return 0;
            }
            else {
                printf("login fallito\n"); // esiste un solo utente con tale username, quindi se non è corretta questa password non controllo altri
                return 1;
            }
        }
    }
    fclose(fptr);
    printf("username non presente\n");
    return 1;
}

void login(sd) {
    char username[1024];
    char password[1024];
    recvMsg(sd, username);
    recvMsg(sd, password);
    loginCheck(username, password);
}

void signup(sd) {
    char username[1024];
    char password[1024];
    recvMsg(sd, username);
    recvMsg(sd, password);
    if (signupControl(username)) {
        printf("username presente\n");
        return;
    }
    signupInsert(username, password);
}

int recvCommand(int sd) {
    int command;
    command = recvNum(sd);
    switch (command) {
    case 1:
        printf("comando ricevuto: signup\n");
        sendNum(sd, 0);
        signup(sd);
        break;
    case 2:
        printf("comando ricevuto: in\n");
        sendNum(sd, 0);
        login(sd);
        break;
    default:
        printf("unknown command\n");
        sendNum(sd, 1);
        return -1;
    }
    return 0;
}

int creaSocket() {
    int ret, sd;
    struct sockaddr_in my_addr;
    char buffer[1024];
    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
    /* Creazione indirizzo di bind */
    memset(&my_addr, 0, sizeof(my_addr)); // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(4242);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    return sd;
}

int main() {
    struct sockaddr_in cl_addr;
    int ret, sd, new_sd, len;
    sd = creaSocket();
    ret = listen(sd, 10);
    if (ret < 0) {
        perror("Errore in fase di bind: \n");
        exit(-1);
    }
    while (1) {
        len = sizeof(cl_addr);

        // Accetto nuove connessioni
        new_sd = accept(sd, (struct sockaddr*)&cl_addr, &len);

        // Attendo risposta
        len = MESSAGE_LEN;
        recvCommand(new_sd);
        if (ret < 0) {
            perror("Errore in fase di ricezione: \n");
            continue;
        }

        // Posso fare strmcp perché invio anche il fine stringa '\0'
        // Invio sempre sul socket 20 byte!
        /*if (strcmp(buffer, "bye") == 0) {
            close(new_sd);
            break;
        }*/
        // Invio risposta
        //ret = send(new_sd, (void*)buffer, len, 0);

        if (ret < 0) {
            perror("Errore in fase di invio: \n");
        }
    }
}