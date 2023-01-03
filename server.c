
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


fd_set master;          //main set: managed with macro 
fd_set read_fds;        //read set: managed from select()
int fdmax;

struct device {
    char* username;
    int port;
    unsigned timestampLogin;
    unsigned timestampLogout;
    bool online;

    int id;
}devices[MAX_DEVICES];

int nDev;

int thisPort;

void fdtInit() {
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);

    fdmax = 0;
    printf("[server] fdt_init: set init done!\n");
}

int deviceSetup(char* username) {

    if (nDev >= MAX_DEVICES)
        return ERROR_CODE;
    struct device* dev = &devices[nDev];

    dev->id = nDev;
    dev->username = malloc(sizeof(username) + 1);
    strcpy(dev->username, username);
    dev->timestampLogin = 0;            //default value: case of signup and not login
    dev->timestampLogout = 0;

    printf("[server] add_dev: added new device!\n"
        "\t dev_id: %d\n"
        "\t username: %s\n",
        dev->id, dev->username
    );
    return nDev++;
}

int findDevice(char* username) {
    int i = 0;
    struct device* dev;
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (!strcmp(dev->username, username)) {
            return i;
        }
    }
    return -1;
}

void updateDevice(int id, int port) {
    struct device* dev = &devices[id];
    dev->port = port;
    dev->timestampLogin = time(NULL);
    dev->timestampLogout = 0;
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


                return 0;
            }
            else {
                return 1;
            }
        }
    }
    fclose(fptr);
    printf("username non presente\n");
    return 1;
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


/*----------------------------------------------------------------------*\
|                     ***     COMANDI SERVER     ***                     |
\*----------------------------------------------------------------------*/

void commandList() {
    int i;
    struct device* dev;
    printf("username\tport\tlogin timestamp\n\n");
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (dev->timestampLogin > dev->timestampLogout) {
            printf("%s\t%u\t%d\n", dev->username, dev->timestampLogin, dev->port);
        }
    }
}
/*----------------------------------------------------------------------*\
|                     ***     COMANDI CLIENT     ***                     |
\*----------------------------------------------------------------------*/
void login(sd) {
    int ret;
    char username[1024];
    char password[1024];
    int id, port;
    recvMsg(sd, username);
    recvMsg(sd, password);
    port = recvNum(sd);
    if (!loginCheck(username, password)) {
        printf("login eseguito con successo\n");
        id = findDevice(username);
        if (id == -1) {
            id = deviceSetup(username);
        }
        printf("%d\n", id);
        updateDevice(id, port);
        sendNum(sd, id);
    }
    else {
        printf("login fallito\n"); // esiste un solo utente con tale username, 
        //quindi se non è corretta questa password non controllo altri
        sendNum(sd, ERROR_CODE);
    }
}

void signup(sd) {
    char username[1024];
    char password[1024];
    recvMsg(sd, username);
    recvMsg(sd, password);
    if (signupControl(username)) {
        printf("username presente\n");
    }
    else
        signupInsert(username, password);

}

void chat(sd) {
    char username[1024];
    int rId;
    struct device* dev;
    recvMsg(sd, username);
    rId = findDevice(username);
    if (rId == -1) {
        printf("Nessun utente registrato con username %s\n", username);
        sendNum(sd, ERROR_CODE);
    }
    dev = &devices[rId];
    sendNum(sd, dev->port);
    printf("l'id di %s e' %d\n", username, dev->port);
}

void out(sd) {
    int id = recvNum(sd);
    struct device* dev = &devices[id];
    dev->timestampLogout = time(NULL);
    printf("Dispositivo %d disconnesso\n", id);
}


void recvCommand(int sd) {
    int command;
    int ret;
    command = recvNum(sd);
    switch (command) {
    case 1:
        printf("command received : signup\n");
        signup(sd);
        break;
    case 2:
        printf("command received : in\n");
        login(sd);
        break;
    case 5:
        printf("command received : chat\n");
        chat(sd);
        break;
    case 7:
        printf("command received : out\n");
        out(sd);
        break;
    default:
        printf("unknown command\n");
    }
    // sendNum(sd, ret);
}

void readCommand() {
    printf("sono qua\n");
    char command[20];
    scanf("%s", command);
    if (!strcmp(command, "list")) {
        commandList();
    }
    else
        printf("! Invalid operation\n");
}

/*----------------------------------------------------------------------*\
|                         ***     MAIN     ***                           |
\*----------------------------------------------------------------------*/


int main(int argc, char* argv[]) {
    struct sockaddr_in cl_addr;
    int ret, sd, new_sd, len, i;


    printf("********************* SERVER AVVIATO ********************\n");

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
    
    fdtInit();
    FD_SET(sd, &master);
    fdmax = sd;


    while (true) {
        printf("Choose operation:\n"
            "- help \n"
            "- list\n"
            "- esc\n");
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("[server] error: select() ");
            exit(-1);
        }
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (i == 0)                      //keyboard
                    readCommand();
                if (i == sd)       //device request  
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
            }
        }
    }

}