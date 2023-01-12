// AGGIUSTARE LA IN e la SIGNUP RIGUARDO LA PORTA NON RICHIESTA
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
    if (sId == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(chatSD);
        //handleDevCrash(sd);
        return;
    }
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
    printf("[SOCKET] Opening connection with %d\n", port);
    /* Creazione socket */
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("[SOCKET] Something went wrong during socket()\n");
        exit(-1);
    }
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("[SOCKET] Something went wrong during setsocket()\n");
        // errore non grave, posso far andare avanti
    }
    /* Creazione indirizzo del server */
    memset(&srv_addr, 0, sizeof(srv_addr)); // Pulizia 
    srv_addr.sin_family = AF_INET;
    srv_addr.sin_port = htons(port);
    inet_pton(AF_INET, "127.0.0.1", &srv_addr.sin_addr);

    if (connect(sd, (struct sockaddr*)&srv_addr, sizeof(srv_addr)) < 0) {
        if (port == -1) {
            printf("<ERROR> Server might be offline, try again later...\n");
            return;
        }
        else {
            printf("<ERROR> Dev might be offline, try again later...\n");
            // dire al server che il dispositivo è offline
            return ERROR_CODE;
        }

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
void chatCommandQuit() {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            sendNum(devices[i].sd, thisDev.id);
            FD_CLR(devices[i].sd, &master);
        }
    }
    nDevChat = 0;
    system("clear");
    return;
}

void chatCommandAdd(sd) {
    int rId, rPort, i, serverSD;
    serverSD = createSocket(-1);
    requestDeviceData(serverSD, &rId, &rPort);

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
    nDevChat++;
    return;
}

void chatCommandUsername() {
    char username[1024];
    int serverSD = createSocket(-1);
    sendCommand(serverSD, COMMAND_DEVICE_DATA);
    printf("[USERS ONLINE]\n");
    while (1) {
        if (recvMsg(serverSD, username) == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            //handleDevCrash(sd);
            return;
        }

        if (username[0] == '\n')
            break;
        printf("%s\n", username);

    }
}
void commandShare() {
    char msg[1024];
    if (nDevChat == 1) {
        printf("[device] type <filename> to share\n");
        system("ls");
        scanf("%s", msg);

        FILE* fp = fopen(msg, "r");
        if (fp == NULL) {
            printf("[SHARE] File '%s' does not exists!\n", msg);
            for (i = 0; i < MAX_DEVICES; i++) {
                if (devices[i].sd != -1) {
                    sendNum(devices[i].sd, SHARE_ERROR);
                }
            }
            return;
        }

        //file exists: sending it
        for (i = 0; i < MAX_DEVICES; i++) {
            if (devices[i].sd != -1) {
                sendNum(devices[i].sd, OK_CODE);
            }
        }
        //get file type from name
        char* name = strtok(msg, ".");
        char* type = strtok(NULL, ".");

        //send type, than file to other device
        printf("[SHARE] sending %s file...\n", type);
        for (j = 0; j < MAX_DEVICES; j++) {
            if (devices[j].sd) {
                sendMsg(devices[j].sd, type);
                send_file(devices[j].sd, fp);
            }
        }
        fclose(fp);
        printf("[SHARE] File shared!\n");
    }
    else
        printf("[SHARE] Command '\\s' is not valid during a group_chat\n");
}

void commandShareRecv() {
    if (nDevChat == 1) {
        printf("[SHARE RECV] Other device is sending you a file: wait...\n");

        //receive OK_CODE to start file transaction, than receive file
        if ((recvNum(sock)) == SHARE_ERROR) {
            printf("[SHARE RECV] File transfer failed: sender error!\n");
            break;
        }

        //get file type [.txt, .c, .h, ecc.]
        char type[WORD_SIZE];
        recvMsg(sock, type);

        //get file and copy in recv.[type]
        printf("[device] receiving %s file...\n", type);
        recv_file(sock, type);
        struct stat st;
        stat("recv.txt", &st);
        int size = st.st_size;
        printf("[device] received %d byte: check 'recv.%s'\n", size, type);
    }
}

void handleChat() {
    char msg[1024];
    int i, code, id, rId, rPort, j;
    nDevChat = 1;
    int serverSD = createSocket(-1); // notifico al server che sono attualmente busy, invierò alla fine della chat un command_not_busy
    sendCommand(serverSD, COMMAND_BUSY);
    sendNum(serverSD, thisDev.id);
    close(serverSD);
    //fgets(msg, 1024, stdin); // necessaria in quanto altrimenti invia
    // una stringa vuota come primo messaggio, non si nota durante l'utilizzo
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            id = i;
            break;
        }
    }
    system("clear");
    FD_SET(devices[i].sd, &master);
    if (devices[i].sd > fdmax) { fdmax = devices[i].sd; }
    //printf("inserisci il messaggio da inviare\n");
    while (true) {
        read_fds = master;
        if (!select(fdmax + 1, &read_fds, NULL, NULL, NULL)) {
            perror("[CHAT] Error: select()\n");
            exit(-1);
        }
        for (i = 0; i <= fdmax; i++) {
            if (FD_ISSET(i, &read_fds)) {
                // sto mandando un messaggio da tastiera
                if (!i) {
                    do {
                        fgets(msg, 1024, stdin);
                    } while (msg[0] == '\n');
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
                        serverSD = createSocket(-1);
                        sendCommand(serverSD, COMMAND_NOT_BUSY);
                        sendNum(serverSD, thisDev.id);
                        close(serverSD);
                        chatCommandQuit();
                        return;
                    case HELP_CODE:
                        chatCommandHelp();
                        break;
                    case ADD_CODE:
                        chatCommandAdd();
                        break;
                    case USER_CODE:
                        chatCommandUsername();
                        //printf("Inserisci il messaggio da inviare\n");
                        break;
                    case SHARE_CODE:
                        chatCommandShare();
                        break;


                    }
                }
                // qualcuno vuole aggiungersi alla chat
                else if (i == listeningChatSocket) {
                    // online ?? 
                    nDevChat++;
                    handleRequest2();
                }
                // ricevuto un messaggio da qualcun'altro
                else if (i != listeningChatSocket) {
                    if (recvMsg(i, msg) == ERROR_CODE) {
                        printf("<ERROR> Something wrong happened\n");
                        FD_CLR(i, &master);
                        close(i);
                        nDevChat--;
                        if (nDevChat == 0) {
                            serverSD = createSocket(-1);
                            sendCommand(serverSD, COMMAND_NOT_BUSY);
                            sendNum(serverSD, thisDev.id);
                            close(serverSD);
                            printf("[CHAT] I'm the only one still in chat, I close\n");
                            sleep(1);
                            system("clear");
                        }
                        //handleDevCrash(sd);
                        return;
                    }
                    code = checkChatCommand(msg);
                    switch (code) {
                    case OK_CODE:
                        printf("%s", msg);
                        break;
                    case QUIT_CODE:
                        // mi faccio inviare da chi esce il suo id per poter cancellare il suo sd
                        rId = recvNum(i);
                        if (rId == ERROR_CODE) {
                            printf("<ERROR> Something wrong happened\n");
                            close(i);
                            //handleDevCrash(sd);
                            return;
                        }
                        nDevChat--;
                        devices[rId].sd = -1;
                        FD_CLR(i, &master);
                        if (nDevChat == 0) {
                            serverSD = createSocket(-1);
                            sendCommand(serverSD, COMMAND_NOT_BUSY);
                            sendNum(serverSD, thisDev.id);
                            close(serverSD);
                            printf("[CHAT] I'm the only one still in chat, I close\n");
                            sleep(1);
                            system("clear");
                            return;
                        }
                        break;
                    case HELP_CODE:
                        chatCommandHelp();
                        break;
                    case ADD_CODE:
                        rId = recvNum(i);
                        if (rId == ERROR_CODE) {
                            printf("<ERROR> Something wrong happened\n");
                            close(i);
                            //handleDevCrash(sd);
                            return;
                        }
                        rPort = recvNum(i);
                        if (rPort == ERROR_CODE) {
                            printf("<ERROR> Something wrong happened\n");
                            close(i);
                            //handleDevCrash(sd);
                            return;
                        }
                        devices[rId].sd = createSocket(rPort);
                        FD_SET(devices[rId].sd, &master);
                        if (devices[rId].sd > fdmax) { fdmax = devices[rId].sd; }
                        sendNum(devices[rId].sd, thisDev.id);
                        nDevChat++;
                        break;
                    case USER_CODE:
                        break;
                    case SHARE_CODE:
                        chatCommandShareRecv();
                        break
                    }
                }
            }
        }
    }
}

void handleChatServer(int serverSD) {
    char msg[1024], buffer[1024], * csId;
    int code, i;
    system("clear");
    printf("[CHAT] You are now chatting with server\n");
    sendNum(serverSD, thisDev.id);
    while (1) {
        do {
            fgets(msg, 1024, stdin);
        } while (msg[0] == '\n');
        code = checkChatCommand(msg);

        if (code == OK_CODE || code == QUIT_CODE) {
            sendMsg(serverSD, msg);
        }
        switch (code) {
        case OK_CODE:
            break;
        case QUIT_CODE:
            close(serverSD);
            return;
        default:
            printf("[CHAT] Invalid command");
        }
    }
}

void handleRequest() {
    int chatSD, sId, serverSD; // sender id
    struct sockaddr_in s_addr;
    socklen_t addrlen = sizeof(s_addr);
    chatSD = accept(listeningChatSocket, (struct sockaddr*)&s_addr, &addrlen);
    sId = recvNum(chatSD);
    if (sId == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(chatSD);
        //handleDevCrash(sd);
        return;
    }
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
    printf("[IN] Insert <server port> <username> <password>\n");
    // ATTENZIONE MODIFICARE QUA, DECOMMENTARE LA SUCCESSIVA E TOGLIERE 
    // QUELLA 2  DOPO
    //scanf("%d", &server.port);
    server.port = 4242;

    serverSD = createSocket(-1);

    sendCommand(serverSD, COMMAND_IN);
    scanf("%s", username);
    scanf("%s", password);
    sendMsg(serverSD, username);
    sendMsg(serverSD, password);
    sendNum(serverSD, thisDev.port);
    ret = recvNum(serverSD);
    if (ret == ERROR_CODE) {
        printf("[IN] Failed\n");
    }
    else {
        printf("[IN] Success\n");
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

    printf("[SIGNUP] Insert <server port> <username> <password>\n");

    //scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);
    server.port = 4242;
    serverSD = createSocket(server.port);

    sendCommand(serverSD, COMMAND_SIGNUP);

    sendMsg(serverSD, username);
    sendMsg(serverSD, password);
    ret = recvNum(serverSD);
    if (ret == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(serverSD);
        //handleDevCrash(sd);
        return;
    }
    if (ret == 0) {
        printf("[SIGNUP] Success\n");
    }
    else
        printf("[SIGNUP] Fail\n");

    close(serverSD);
}

void commandHanging() {
    char username[1024];
    int numPending;
    char timestamp[80];
    int serverSD = createSocket(-1);
    sendCommand(serverSD, COMMAND_HANGING);
    sendNum(serverSD, thisDev.id);
    while (true) {
        if (recvMsg(serverSD, username) == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            //handleDevCrash(sd);
            return;
        }
        if (username[0] == '\n')
            break;
        numPending = recvNum(serverSD);
        if (numPending == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            //handleDevCrash(sd);
            return;
        }
        if (recvMsg(serverSD, timestamp) == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            //handleDevCrash(sd);
            return;
        }


        // Format time, "ddd yyyy-mm-dd hh:mm:ss zzz"
        printf("%s %d %s\n", username, numPending, timestamp);
    }
}

// CHAT

int requestDeviceData(int serverSD, int* id, int* port) {
    char username[1024];
    printf("[CHAT] Insert <username>\n");
    sendCommand(serverSD, COMMAND_CHAT);
    scanf("%s", username);
    sendMsg(serverSD, username);
    *id = recvNum(serverSD);
    if (*id == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(serverSD);
        //handleDevCrash(sd);
        return -1;
    }
    *port = recvNum(serverSD);
    if (*port == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(serverSD);
        //handleDevCrash(sd);
        return -1;
    }
}

void commandChat() {
    int rPort, rId;
    int serverSD = createSocket(-1);

    if (requestDeviceData(serverSD, &rId, &rPort) == ERROR_CODE) {
        close(serverSD);
        //handleDevCrash(sd);
        return;
    }
    if (rId == USER_NOT_FOUND) {
        printf("[CHAT] Username not found\n");
        return;
    }
    if (rPort == USER_OFFLINE) {
        printf("[CHAT] User is offline\n");
        sleep(1);
        handleChatServer(serverSD);
        return;
    }
    if (rPort == USER_BUSY) {
        printf("[CHAT] User is busy\n");
        sleep(1);
        handleChatServer(serverSD);
        return;
    }
    devices[rId].sd = createSocket(rPort);
    if (devices[rId].sd == ERROR_CODE) { // il dispositivo con cui volevo dialogare è crashato, lo notifico al server
        serverSD = createSocket(-1);     // e avvio la chat offline
        sendCommand(serverSD, USER_OFFLINE);
        sendNum(serverSD, rId);
        sleep(1);
        handleChatServer(serverSD);
        return;
    }
    sendNum(devices[rId].sd, thisDev.id);
    handleChat();
    serverSD = createSocket(-1);
    sendCommand(serverSD, COMMAND_NOT_BUSY);
    sendNum(serverSD, thisDev.id);
    close(serverSD);
}
// OUT

void commandOut() {
    int serverSD;

    serverSD = createSocket(server.port);

    sendCommand(serverSD, COMMAND_OUT);
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
        printf("[READ COMMAND] Invalid operation\n");
    }
    else {

        scanf("%s", command);
        if (!strcmp(command, "hanging")) {
            commandHanging();
            return;
        }
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
    system("clear");
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
                "- signup <server port> <username> <password>\n"
                "- in <server port> <username> <password>\n"
                "> ");
        else
            printf("Choose operation:\n"
                "- hanging\n"
                "- show <username>\n"
                "- chat <username>\n"
                "- share <file_name>\n"
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