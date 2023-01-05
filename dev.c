// AGGIUSTARE LA IN RIGUARDO LA PORTA NON RICHIESTA

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
    int sd;
}devices[20];

struct serverStruct {
    int sd;
    int port;
};

struct dev thisDev;

int nDevChat;

struct serverStruct server;

int listeningChatSocket;


fd_set master;          //main set: managed with macro 
fd_set read_fds;        //read set: managed from select() 
int fdmax;

/*--**********************************--*\
|       *** FUNZIONI GENERALI ***        |
\*--**********************************--*/

void handleRequest2() {
    int chatSD, sId; // sender id
    struct sockaddr_in s_addr;
    socklen_t addrlen = sizeof(s_addr);
    chatSD = accept(listeningChatSocket, (struct sockaddr*)&s_addr, &addrlen);
    sId = recvNum(chatSD);
    devices[sId].sd = chatSD;
    FD_SET(chatSD, &master);
    if (chatSD > fdmax) { fdmax = chatSD; }

}

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

int checkChatCommand(char* cmd) {
    //check if user typed a command while chatting: return an INT with COMMAND_CODE

    if (!strncmp(cmd, "\\q", 2)) {
        return QUIT_CODE;
    }
    else if (!strncmp(cmd, "\\u", 2)) {
        return USER_CODE;
    }
    else if (!strncmp(cmd, "\\a", 2)) {
        return ADD_CODE;
    }
    else if (!strncmp(cmd, "\\s", 2)) {
        return SHARE_CODE;
    }
    else if (!strncmp(cmd, "\\h", 2)) {
        return HELP_CODE;
    }
    else if (!strncmp(cmd, "\\c", 2)) {
        return CLEAR_CODE;
    }

    return OK_CODE;     //no command: just a message
}

void chatCommandHelp() {
    printf(
        "[HELP] type a message + ENTER to send it\n"
        "COMMANDS:\n"
        "\\a [user]: add [user] to chat\n"
        "\\s [file] : share [file] to users in chat\n"
        "\\q : quit chat\n");
}

void chatCommandQuit(sd) {
    FD_CLR(sd, &master);
    return;
}

void chatCommandAdd(sd) {
    int rId, rPort, i;
    requestDeviceData(&rId, &rPort);

    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            sendNum(devices[i].sd, rId);
            sendNum(devices[i].sd, rPort);
        }
    }
    devices[rId].sd = createSocket(rPort);
    sendNum(devices[rId].sd, thisDev.id);
    FD_SET(devices[rId].sd, &master);
    if (devices[rId].sd > fdmax) { fdmax = devices[rId].sd; }
    return;
}

void handleChat() {
    char msg[1024];
    int i, code, id, rId, rPort;
    fgets(msg, 1024, stdin); // necessaria in quanto altrimenti invia
    // una stringa vuota come primo messaggio, non si nota durante l'utilizzo
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            id = i;
            break;
        }
    }
    printf("e da qua\n");
    FD_SET(devices[i].sd, &master);
    if (devices[i].sd > fdmax) { fdmax = devices[i].sd; }
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
                    fgets(msg, 1024, stdin);
                    code = checkChatCommand(msg);
                    for (i = 0; i < MAX_DEVICES; i++) {
                        if (devices[i].sd != -1) {
                            sendMsg(devices[i].sd, msg);
                        }
                    }
                    switch (code) {
                    case OK_CODE:
                        break;
                    case QUIT_CODE:
                        for (i = 0; i < MAX_DEVICES; i++) {
                            if (devices[i].sd != -1) {
                                printf("invio un messaggio su %d\n", devices[i].sd);
                                chatCommandQuit(devices[i].sd); // qui non dovrebbe servire sd
                            }
                        }

                        return;
                    case HELP_CODE:
                        chatCommandHelp();
                        break;
                    case ADD_CODE:
                        chatCommandAdd();
                        break;
                        /*if (!strncmp(msg, "\\u", 2)) {
                            chatCommandUsername();
                        }
                        if (!strncmp(msg, "\\a", 2)) {
                            chatCommandAdd();
                        }
                        if (!strncmp(msg, "\\s", 2)) {
                            chatCommandShare();
                        }
                        }*/
                    }
                }
                else if (i == listeningChatSocket) {
                    handleRequest2();
                }
                else if (i != listeningChatSocket) {
                    recvMsg(i, msg);
                    code = checkChatCommand(msg);
                    switch (code) {
                    case OK_CODE:
                        printf("%s", msg);
                        break;
                    case QUIT_CODE:
                        FD_CLR(i, &master);
                        return;
                    case HELP_CODE:
                        chatCommandHelp();
                        break;
                    case ADD_CODE:
                        rId = recvNum(i);
                        rPort = recvNum(i);
                        devices[rId].sd = createSocket(rPort);
                        FD_SET(devices[rId].sd, &master);
                        if (devices[rId].sd > fdmax) { fdmax = devices[rId].sd; }

                        sendNum(devices[rId].sd, thisDev.id);
                        break;
                        /*if (!strncmp(msg, "\\u", 2)) {
                            chatCommandUsername();
                        }
                        if (!strncmp(msg, "\\a", 2)) {
                            chatCommandAdd();
                        }
                        if (!strncmp(msg, "\\s", 2)) {
                            chatCommandShare();
                        }
                        }*/
                    }
                }
            }
        }
    }
}

void handleRequest() {
    int chatSD, sId; // sender id
    struct sockaddr_in s_addr;
    socklen_t addrlen = sizeof(s_addr);
    chatSD = accept(listeningChatSocket, (struct sockaddr*)&s_addr, &addrlen);
    sId = recvNum(chatSD);
    devices[sId].sd = chatSD;
    handleChat();
    close(devices[sId].sd);
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
    printf("Insert [server port] [username] [password]\n");
    // ATTENZIONE MODIFICARE QUA, DECOMMENTARE LA SUCCESSIVA E TOGLIERE 
    // QUELLA 2  DOPO
    //scanf("%d", &server.port);
    server.port = 4242;
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
        printf("il mio id e %d\n", ret);
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

int requestDeviceData(int* id, int* port) {
    char username[1024];
    int serverSD, Port;
    printf("Insert [username]\n");
    serverSD = createSocket(-1);
    sendCommand(serverSD, 5);
    scanf("%s", username);
    sendMsg(serverSD, username);
    *id = recvNum(serverSD);
    *port = recvNum(serverSD);

}

void commandChat() {
    int rPort, rId;
    requestDeviceData(&rId, &rPort);
    devices[rId].sd = createSocket(rPort);
    sendNum(devices[rId].sd, thisDev.id);
    handleChat();
}
// OUT

void commandOut() {
    int serverSD;

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
    int i;
    if (argc != 2) {
        printf("Syntax error!\nCorrect syntax is: ./dev [port]\n");
        exit(-1);
    }
    thisDev.port = atoi(argv[1]);
    thisDev.online = false;
    for (i = 0; i < MAX_DEVICES; i++) {
        devices[i].sd = -1;
    }
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
                if (!i) {                              //keyboard
                    readCommand();
                }
                else if (i == listeningChatSocket) {   //handle request (server or other device)
                    handleRequest();
                }
            }
        }
    }
}