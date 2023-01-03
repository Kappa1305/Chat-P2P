
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
    char* username;
    int port;
    char timestampLogin[TIMER_SIZE];
    char timestampLogout[TIMER_SIZE];

    int id;
}devices[MAX_DEVICES];

int nDev;

int thisPort;

int FindIdDevice(char username[1024]) {
    int c;
    for (c = 0; c < nDev; c++) {
        if (!strcmp(username, devices[c].username))
            return c;
    }
    // username non presente nella lista di devices
    return -1;
}

int deviceSetup(const char* username) {

    if (nDev >= MAX_DEVICES)
        return ERROR_CODE;
    nDev++;
    struct device* dev = &devices[nDev];

    dev->id = nDev;
    dev->username = malloc(sizeof(username) + 1);
    strcpy(dev->username, username);
    strcpy(dev->timestampLogin, "00:00:00");            //default value: case of signup and not login
    strcpy(dev->timestampLogout, "00:00:00");


    printf("[server] add_dev: added new device!\n"
        "\t dev_id: %d\n"
        "\t username: %s\n",
        dev->id, dev->username
    );
}

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

int login(sd) {
    int ret;
    char username[1024];
    char password[1024];
    struct device* dev;
    recvMsg(sd, username);
    recvMsg(sd, password);
    ret = loginCheck(username, password);

    if (ret == -1) {
        sendNum(sd, 1);
    }
    dev = &devices[ret];
    // registro orario di login
    
    sendNum(sd, 0);
}

int signup(sd) {
    char* username;
    char password[1024];
    recvMsg(sd, username);
    recvMsg(sd, password);
    if (signupControl(username)) {
        printf("username presente\n");
        return 1;
    }
    deviceSetup(username);
    signupInsert(username, password);
    return 0;
}

void recvCommand(int sd) {
    int command;
    int ret;

    command = recvNum(sd);
    switch (command) {
    case 1:
        printf("comando ricevuto: signup\n");
        ret = signup(sd);
        break;
    case 2:
        printf("comando ricevuto: in\n");
        ret = login(sd);
        break;
    default:
        printf("unknown command\n");
        ret = 1;
    }
    sendNum(sd, ret);

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
    my_addr.sin_port = htons(thisPort);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    return sd;
}

int main(int argc, char* argv[]) {
    struct sockaddr_in cl_addr;
    int ret, sd, new_sd, len;
        printf("b\n");

    switch (argc) {
    case 1:
        thisPort = 4242;
        break;
    case 2:
        thisPort = atoi(argv[1]);
    default:
        printf("Syntax error\n");
        exit(-1);
    }
    sd = creaSocket();
    ret = listen(sd, 10);
    if (ret < 0) {
        perror("Something went wrong during bind: \n");
        exit(-1);
    }
    while (1) {
        len = sizeof(cl_addr);

        // Accetto nuove connessioni
        new_sd = accept(sd, (struct sockaddr*)&cl_addr, &len);

        // Attendo risposta
        len = MESSAGE_LEN;
        recvCommand(new_sd);
    /*printf("a\n");
    ret = 0;
        if (ret < 0) {
                printf("qua\n");

            perror("Errore in fase di ricezione: \n");
            continue;
        }
                        printf("qua!\n");
*/
        // Posso fare strmcp perché invio anche il fine stringa '\0'
        // Invio sempre sul socket 20 byte!
        /*if (strcmp(buffer, "bye") == 0) {
            close(new_sd);
            break;
        }*/
        // Invio risposta
        //ret = send(new_sd, (void*)buffer, len, 0);


    }
}

// << Il progetto serve se non ce l'hai, non serve se ce l'hai>>