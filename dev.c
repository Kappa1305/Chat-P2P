

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

/*--**********************************--*\
|        *** STRUTTURE DATI ***          |
\*--**********************************--*/
struct dev {
    int port;
    char* username;
    bool online;
    int id;
    struct sockaddr_in addr;
};

struct serverStruct {
    int sd;
    int port;
};

struct dev thisDev;

int listeningChatSocket;

struct serverStruct server;


fd_set master;          //main set: managed with macro 
fd_set read_fds;        //read set: managed from select() 
int fdmax;

/*--**********************************--*\
|       *** FUNZIONI GENERALI ***        |
\*--**********************************--*/

void fdtInit() {
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);

    fdmax = 0;

    printf("[fdt_init] set init done...\n");
}

void createListeningSocket() {
    listeningChatSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningChatSocket == -1) {
        perror("Something went wrong during socket()\n");
        exit(-1);
    }
    if (setsockopt(listeningChatSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("Something went wrong during setsocket()\n");
        // errore non grave, posso far andare avanti
    }

    memset(&thisDev.addr, 0, sizeof(thisDev.addr)); // Pulizia 
    thisDev.addr.sin_family = AF_INET;
    thisDev.addr.sin_port = htons(thisDev.port);
    thisDev.addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listeningChatSocket, (struct sockaddr*)&thisDev.addr, sizeof(thisDev.addr)) == -1) {
        perror("[device] Error bind: \n");
        exit(-1);
    }
    listen(listeningChatSocket, 10);
    FD_SET(listeningChatSocket, &master);
    if (listeningChatSocket > fdmax) { fdmax = listeningChatSocket; }
}

int createSocket(int port) {
    int sd;
    struct sockaddr_in srv_addr;
    if (port == -1)
        port = server.port;
    printf("apro connessione con %d\n", port);
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
    srv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    if (connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        perror("Something went wrong during connect: \n");
        exit(-1);
    }
    return sd;
}

void sendCommand(int serverSD, int command) {
    sendNum(serverSD, command);
}

void handleChat(int sd) {
    char msg[1024];
    int i;
    FD_SET(sd, &master);
    if(sd > fdmax){fdmax = sd;}
    printf("inserisci il messaggio da inviare\n");
    while (true) {
        read_fds = master;
        if (!select(fdmax + 1, &read_fds, NULL, NULL, NULL)) {
            perror("[handle_chat] Error: select()\n");
            exit(-1);
        }
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (!i) {
                    scanf("%s", msg);
                    sendMsg(sd, msg);
                }
                else if(i != listeningChatSocket){
                    recvMsg(sd, msg);
                    printf("%s\n", msg);
                }
            }
        }
    }
}
void handleRequest() {
    int chatSD;
    struct sockaddr_in s_addr;
    socklen_t addrlen = sizeof(s_addr);
    chatSD = accept(listeningChatSocket, (struct sockaddr*)&s_addr, &addrlen);
    printf("fatto\n");
    handleChat(chatSD);
}

/*--**********************************--*\
|        ***     COMANDI     ***         |
\*--**********************************--*/

// IN

void commandIn() {
    int ret;
    int serverSD;
    char username[1024];
    char password[1024];
    char buffer[1024];
    printf("Insert [server port] [username] [password]\n");
    scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);

    serverSD = createSocket(-1);

    sendCommand(serverSD, 2);
    sendMsg(serverSD, username);
    sendMsg(serverSD, password);
    sendNum(serverSD, thisDev.port);
    ret = recvNum(serverSD);
    printf("%d\n", ret);
    if (ret == ERROR_CODE) {
        printf("login fallito\n");
    }
    else {
        printf("login effettuato con successo\n");
        printf("********************* DEVICE %d ONLINE ********************\n", ret);
        createListeningSocket();
        thisDev.id = ret;
        thisDev.online = true;
    }
    close(serverSD);

}

// SIGNUP

void commandSignup() {
    char username[1024];
    char password[1024];
    int serverSD, ret;

    printf("Insert [server port] [username] [password]\n");

    scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);

    serverSD = createSocket(server.port);

    sendCommand(serverSD, 1);

    sendMsg(serverSD, username);
    sendMsg(serverSD, password);

    ret = recvNum(serverSD);
    if (ret == 0) {
        printf("registrazione effettuata con successo\n");
    }
    else
        printf("registrazione fallita\n");

    close(serverSD);
}

// CHAT

void commandChat() {
    char username[1024];
    int serverSD, rPort, chatSD;
    printf("Insert [username]\n");

    serverSD = createSocket(-1);

    sendCommand(serverSD, 5);

    scanf("%s", username);
    sendMsg(serverSD, username);
    rPort = recvNum(serverSD);
    chatSD = createSocket(rPort);
    handleChat(chatSD);
}
// OUT

void commandOut() {
    char username[1024];
    char password[1024];
    int serverSD, ret;

    serverSD = createSocket(server.port);

    sendCommand(serverSD, 7);
    sendNum(serverSD, thisDev.id);
    thisDev.online = false;
    printf("********************* DEVICE %d OFFLINE ********************\n", thisDev.id);
    close(listeningChatSocket);
    FD_CLR(listeningChatSocket, &master);
    close(serverSD);
}

void readCommand() {
    char command[20];

    if (!thisDev.online) {

        scanf("%s", command);
        if (!strcmp(command, "signup")) {
            commandSignup();
            return;
        }
        if (!strcmp(command, "in")) {
            commandIn();
            return;
        }
        // default
        printf("! Invalid operation\n");
    }
    else {

        scanf("%s", command);
        if (!strcmp(command, "chat")) {
            commandChat();
            return;
        }
        if (!strcmp(command, "out")) {
            commandOut();
            return;
        }
        // default
        printf("! Invalid operation\n");
    }
}

/*--**********************************--*\
|         ***      MAIN      ***         |
\*--**********************************--*/


int main(int argc, char* argv[]) {
    int ret, len, i;
    if (argc != 2) {
        printf("Syntax error!\nCorrect syntax is: ./dev [port]\n");
        exit(-1);
    }
    len = 20;
    thisDev.port = atoi(argv[1]);
    thisDev.online = false;

    fdtInit();
    FD_SET(listeningChatSocket, &master);
    fdmax = listeningChatSocket;

    while (1) {

        if (!thisDev.online)
            printf("Choose operation:\n"
                "- signup [server port] [username] [password]\n"
                "- in [server port] [username] [password]\n"
                "> ");
        else
            printf("Choose operation:\n"
                "- hanging\n"
                "- show [username]\n"
                "- chat [username]\n"
                "- share [file_name]\n"
                "- out\n"

                "> ");
        read_fds = master;
        if (select(fdmax + 1, &read_fds, NULL, NULL, NULL) == -1) {
            perror("[device] error select() ");
            exit(-1);
        }
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                if (!i)                              //keyboard
                    readCommand();
            
            else if (i == listeningChatSocket)      //handle request (server or other device)
                handleRequest();
            }
        }
    }
}