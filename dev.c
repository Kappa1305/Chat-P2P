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
#include <sys/stat.h>

#include "utility.c"

#define MESSAGE_LEN 20 // Lunghezza del messaggio dal client

struct stat st = { 0 };

/*--**********************************--*\
|        *** STRUTTURE DATI ***          |
\*--**********************************--*/

struct dev {
    int port;
    char* username;
    bool logged; // utilizzato per controllare se stampare il prompt in/signup oppure chat/show/hanging...
    int id;
    struct sockaddr_in addr;
    int sd; // socket di comunicazione tra thisDev e altro dispositivo
}devices[MAX_DEVICES];

struct serverStruct {
    int sd;
    int port;
};

struct dev thisDev; // dispositivo collegato al processo in esecuzione

struct serverStruct server;

int nDevChat; // numero di device connessi alla chat

int listeningSocket;


fd_set master;
fd_set read_fds;
int fdmax;

/*--**********************************--*\
|       *** FUNZIONI GENERALI ***        |
\*--**********************************--*/


void fdtInit() {
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);

    fdmax = 0;

    printf("[FDT INIT] Done\n");
}


void createListeningSocket() {
    listeningSocket = socket(AF_INET, SOCK_STREAM, 0);
    if (listeningSocket == -1) {
        perror("<ERROR> Something went wrong during socket()\n");
        exit(-1);
    }
    // necessario per poter utilizzare le porte più volte, senza di questo ogni volta che riavvio il
    // server o il dispositivo dovrei aspettare qualche secondo per far sì che il Sistema Operativo
    // ripulisca la memoria delle porte (?)
    if (setsockopt(listeningSocket, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("<WARNING> Something went wrong during setsocket()\n");
        // errore non grave, posso far andare avanti
    }

    // Pulizia 
    memset(&thisDev.addr, 0, sizeof(thisDev.addr));
    thisDev.addr.sin_family = AF_INET;
    thisDev.addr.sin_port = htons(thisDev.port);
    thisDev.addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(listeningSocket, (struct sockaddr*)&thisDev.addr, sizeof(thisDev.addr)) == -1) {
        perror("<ERROR> Something went wrong during bind()\n");
        exit(-1);
    }
    listen(listeningSocket, 10);
    FD_SET(listeningSocket, &master);
    if (listeningSocket > fdmax) { fdmax = listeningSocket; }
    printf("[CREATE LISTENING SOCKET] Done\n");
}

// chiamata dal dispositivo aggiunto alla chat quando un dispositivo 
//(non quello che ha chiamato la \a) desidera connettersi oppure quando 
// il server vuole notificare che qualcuno ha effettuato la show su dei 
// messaggi pendenti del thisDev

void handleRequest(bool inChat);

int requestDeviceData(int serverSD, int* id, int* port);

// controlla se l'user ha inserito un comando nella chat, in caso affermativo ne restituisce il codice
int checkChatCommand(char* cmd) {

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
    // default: nessun comando, è un semplica messaggio
    return OK_CODE;
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
            // informo tutti gli altri dispositivi qual è l'id del dispositivo che è uscito dalla chat
            sendNum(devices[i].sd, thisDev.id);
            // tolgo il socket degli altri dispositivi da quelli che ascolto
            FD_CLR(devices[i].sd, &master);
            sleep(1);
            close(devices[i].sd);
            devices[i].sd = -1;
        }
    }
    nDevChat = 0;
    system("clear");
    return;
}

// chiamata quando qualche errore è avvenuto durante la add in modo che gli altri dispositivi non stiano
// infinitamente l'id del dispositivo da aggiungere
void notifyAddError() {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            sendNum(devices[i].sd, ADD_ERROR);
        }
    }
}

void chatCommandAdd(sd) {
    // dispositivo chiede al server id e porta del dispositivo da aggiungere
    // inviandogli l'username
    int rId, rPort, i, serverSD;
    serverSD = createSocket(server.port);
    sendNum(serverSD, COMMAND_ADD);
    // se il server è offline invio a tutti un messaggio di errore
    // che indica che la add non può essere completata
    if (serverSD == ERROR_CODE) {
        notifyAddError();
        return;
    }
    if (requestDeviceData(serverSD, &rId, &rPort) == ERROR_CODE) {
        notifyAddError();
        return;
    }
    if (rId == USER_NOT_FOUND) {
        notifyAddError();
        printf("[ADD] Username not found\n");
        return;
    }
    if (rPort == USER_OFFLINE) {
        notifyAddError();
        printf("[ADD] User is offline\n");
        return;
    }
    if (rPort == USER_BUSY) {
        notifyAddError();
        printf("[ADD] User is offline\n");
        return;
    }
    //  invia a tutti i dispositivi nella chat id e porta del nuovo 
    // dispositivo per far sì che creino con esso un socket
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            sendNum(devices[i].sd, rId);
            sendNum(devices[i].sd, rPort);
        }
    }

    // crea un socket col dispositivo aggiunto e lo informa del suo id
    devices[rId].sd = createSocket(rPort);
    sendNum(devices[rId].sd, thisDev.id);
    sendNum(devices[rId].sd, 1);
    // aggiunge il socket del nuovo dispositivo a quelli di ascolto
    FD_SET(devices[rId].sd, &master);
    if (devices[rId].sd > fdmax) { fdmax = devices[rId].sd; }

    nDevChat++;

    return;
}

void chatCommandUsername() {
    char username[1024];
    int serverSD = createSocket(server.port);
    sendNum(serverSD, COMMAND_DEVICE_DATA);
    printf("[USERS ONLINE]\n");
    while (1) {
        if (recvMsg(serverSD, username) == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            return;
        }

        if (username[0] == '\n')
            break;
        printf("%s\n", username);

    }
}
void chatCommandShare() {
    int i, ret;
    char msg[1024];
    // leggo da tastiera il nome del file
    printf("[device] type <filename> to share\n");
    system("ls");
    scanf("%s", msg);

    // apro il file
    FILE* fp = fopen(msg, "r");
    // controllo che sia andato a buon fine, altrimenti mando errore
    // a tutti quelli in ascolto
    if (fp == NULL) {
        printf("[SHARE] File '%s' does not exists!\n", msg);
        for (i = 0; i < MAX_DEVICES; i++) {
            if (devices[i].sd != -1) {
                sendNum(devices[i].sd, SHARE_ERROR);
            }
        }
        return;
    }

    // tutto bene, invio a tutti un ok_code, il tipo del file e il file
    char* type = strtok(NULL, ".");
    printf("[SHARE] sending %s file...\n", type);

    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            sendNum(devices[i].sd, OK_CODE);
            sendMsg(devices[i].sd, type);
            ret = sendFile(devices[i].sd, fp);

            if (ret == ERROR_CODE) {
                printf("<ERROR> Something wrong happened\n");
                return;
            }
            sleep(1);
            // se non inizializassi il puntatore a carattere nel file la prossima lettura inizierebbe dall'ultimo carattere
            // => leggerebbe 0 bytes
            fseek(fp, 0, SEEK_SET);
        }
    }

    printf("[SHARE] File shared!\n");
}


void readChat(int id) {
    char filename[WORD_SIZE];
    sprintf(filename, "chat_device_%d/chat_with_%d.txt", thisDev.id, id);
    FILE* fp = fopen(filename, "r");
    if (fp) {
        char buff[BUFFER_SIZE];
        while (fgets(buff, BUFFER_SIZE, fp) != NULL)
            printf("%s", buff);
        fclose(fp);
    }
}

void chatCommandShareRecv(sd) {
    printf("[SHARE RECV] Other device is sending you a file: wait...\n");
    //receive OK_CODE to start file transaction, than receive file
    int ret;
    recvNum(sd, &ret);
    if (ret == SHARE_ERROR) {
        printf("[SHARE RECV] File transfer failed: sender error!\n");
        return;
    }

    //get file type [.txt, .c, .h, ecc.]
    char* type = "txt";
    recvMsg(sd, type);
    //get file and copy in recv.[type]
    printf("[device] receiving %s file...\n", type);
    recvFile(sd, type);
    struct stat st;
    stat("recv.txt", &st);
    int size = st.st_size;
    printf("[device] received %d byte: check 'recv.%s'\n", size, type);
}

// funzione chiamata quando un device in attesa di una recv in chat riceve un errore
// viene considerato per ipotesi semplificativa un crash (ctrl-c) del dispositivo
int handleDevCrash(int sd) {
    int serverSD;
    printf("<ERROR> Something wrong happened: dev in chat crashed\n");

    FD_CLR(sd, &master);
    close(sd);

    nDevChat--;

    // se eravamo solo in due in chat la termino
    if (nDevChat == 0) {
        serverSD = createSocket(server.port);

        if (serverSD != ERROR_CODE) {
            sendNum(serverSD, COMMAND_NOT_BUSY);
            sendNum(serverSD, thisDev.id);
            close(serverSD);
        }

        printf("[CHAT] I'm the only one still in chat, I close\n");
        sleep(1);
        system("clear");
        return 1;
    }
    return 0;
}

void addMsgData(char* msg) {
    char tmp[1024];
    strcpy(tmp, msg);
    struct tm ts;
    time_t msgTime;
    char buf[1024];
    time(&msgTime);
    ts = *localtime(&msgTime);
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    sprintf(msg, "%s [%s]: %s", thisDev.username, buf, tmp);
}

void handleChat(int id, bool groupChat) {
    char msg[1024];
    int i, j, code, rId, rPort;
    nDevChat = 1;
    // notifico al server che sono attualmente busy, invierò alla fine della chat un command_not_busy
    int serverSD = createSocket(server.port);
    sendNum(serverSD, COMMAND_BUSY);
    sendNum(serverSD, thisDev.id);
    close(serverSD);
    system("clear");
    if(!groupChat)
        readChat(id);
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
                    for (j = 0; j < MAX_DEVICES; j++) {
                        if (devices[j].sd != -1) {
                            sendNum(devices[j].sd, code);
                        }
                    }
                    switch (code) {
                        // ho inviato un messaggio
                    case OK_CODE:
                        addMsgData(msg);
                        for (j = 0; j < MAX_DEVICES; j++) {
                            if (devices[j].sd != -1) {
                                sendMsg(devices[j].sd, msg);
                            }
                        }
                        if (nDevChat == 1) {
                            char filename[WORD_SIZE];
                            sprintf(filename, "./chat_device_%d/chat_with_%d.txt", thisDev.id, id);
                            FILE* fp = fopen(filename, "a");
                            if (fp) {
                                fprintf(fp, "%s", msg);
                                fclose(fp);
                            }
                        }

                        break;

                    case QUIT_CODE:
                        serverSD = createSocket(server.port);
                        sendNum(serverSD, COMMAND_NOT_BUSY);
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
                else if (i == listeningSocket) {
                    // online ?? 
                    handleRequest(true);
                    nDevChat++;
                }
                // ricevuto un messaggio da qualcun'altro
                else if (i != listeningSocket) {
                    recvNum(i, &code);
                    if (code == ERROR_CODE) {
                        // la handleDevCrash restituisce 1 se devo terminare la chat
                        if (handleDevCrash(i)) {
                            return;
                        }
                        continue;
                    }
                    // dispositivo che mi stava scrivendo è crashato


                    switch (code) {

                        // ho ricevuto un messaggio
                    case OK_CODE:
                        if (recvMsg(i, msg) == ERROR_CODE) {
                            // la handleDevCrash restituisce 1 se devo terminare la chat
                            if (handleDevCrash(i)) {
                                return;
                            }
                            continue;
                        }
                        printf("%s", msg);
                        if (nDevChat == 1) {
                            char filename[WORD_SIZE];
                            sprintf(filename, "./chat_device_%d/chat_with_%d.txt", thisDev.id, id);
                            FILE* fp = fopen(filename, "a");
                            if (fp) {
                                fprintf(fp, "%s", msg);
                                fclose(fp);
                            }
                        }
                        break;

                        // qualcuno vuole uscire dalla chat
                    case QUIT_CODE:
                        // mi faccio inviare da chi esce il suo id per poter cancellare il suo sd
                        recvNum(i, &rId);
                        if (rId == ERROR_CODE) {
                            if (handleDevCrash(i)) {
                                return;
                            }
                            continue;
                        }
                        // tolgo il dispositivo da quelli ascoltati
                        nDevChat--;
                        devices[rId].sd = -1;
                        FD_CLR(i, &master);
                        close(i);

                        if (nDevChat == 0) {
                            // segnalo al server che sono tornato disponibile a chattare
                            serverSD = createSocket(server.port);
                            sendNum(serverSD, COMMAND_NOT_BUSY);
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
                        recvNum(i, &rId);

                        // gestione crash del dispositivo su i
                        if (rId == ERROR_CODE) {
                            if (handleDevCrash(i)) {
                                return;
                            }
                            continue;
                        }

                        // gestione server offline, non si può aggiungere nessuno
                        if (rId == ADD_ERROR) {
                            continue;
                        }

                        recvNum(i, &rPort);
                        // gestione crash del dispositivo su i
                        if (rPort == ERROR_CODE) {
                            if (handleDevCrash(i)) {
                                return;
                            }
                            continue;
                        }

                        // creo un socket con il dispositivo aggiunto e lo aggiungo a quelli ascoltati
                        devices[rId].sd = createSocket(rPort);
                        FD_SET(devices[rId].sd, &master);
                        if (devices[rId].sd > fdmax) { fdmax = devices[rId].sd; }

                        // informo il dispositivo aggiunto del mio id
                        sendNum(devices[rId].sd, thisDev.id);

                        nDevChat++;

                        break;

                    case USER_CODE:
                        // nothing to do here
                        break;

                    case SHARE_CODE:
                        chatCommandShareRecv(i);
                        break;
                    }
                }
            }
        }
    }
}

void handleChatServer(int serverSD) {
    char msg[1024];
    int code;
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

void handleRequest(bool inChat) {
    int chatSD, sId; // sender id
    int groupChat;
    struct sockaddr_in s_addr;
    socklen_t addrlen = sizeof(s_addr);
    chatSD = accept(listeningSocket, (struct sockaddr*)&s_addr, &addrlen);
    recvNum(chatSD, &sId);
    if (sId == COMMAND_SHOW) {
        printf("[CHAT] Someone read your pending messages\n");
        close(chatSD);
        return;
    }
    if (sId == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(chatSD);
        return;
    }
    devices[sId].sd = chatSD;
    recvNum(devices[sId].sd, &groupChat);
    FD_SET(devices[sId].sd, &master);
    if (devices[sId].sd > fdmax) { fdmax = devices[sId].sd; }
    if (!inChat) {
        handleChat(sId, groupChat);
        close(devices[sId].sd);
    }
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

    serverSD = createSocket(server.port);
    if (serverSD == ERROR_CODE) {
        return;
    }
    sendNum(serverSD, COMMAND_IN);
    scanf("%s", username);
    scanf("%s", password);
    sendMsg(serverSD, username);
    sendMsg(serverSD, password);
    sendNum(serverSD, thisDev.port);
    recvNum(serverSD, &ret);
    if (ret == ERROR_CODE) {
        printf("[IN] Failed\n");
    }
    else {
        printf("[IN] Success\n");
        printf("********************* DEVICE %d ONLINE ********************\n", ret);
        createListeningSocket();
        thisDev.id = ret;
        thisDev.logged = true;
        thisDev.username = malloc(sizeof(username));
        strcpy(thisDev.username, username);
        struct stat st = { 0 };
        char dir_path[15];
        sprintf(dir_path, "./chat_device_%d", thisDev.id);
        if (stat(dir_path, &st) == -1)
            mkdir(dir_path, 0700);
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

    sendNum(serverSD, COMMAND_SIGNUP);

    sendMsg(serverSD, username);
    sendMsg(serverSD, password);
    recvNum(serverSD, &ret);
    if (ret == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(serverSD);
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
    int serverSD = createSocket(server.port);
    sendNum(serverSD, COMMAND_HANGING);
    sendNum(serverSD, thisDev.id);
    while (true) {
        if (recvMsg(serverSD, username) == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            return;
        }
        if (username[0] == '\n')
            break;
        recvNum(serverSD, &numPending);
        if (numPending == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            return;
        }
        if (recvMsg(serverSD, timestamp) == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
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
    scanf("%s", username);
    if (sendMsg(serverSD, username) == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(serverSD);
        return ERROR_CODE;
    }
    recvNum(serverSD, id);
    if (*id == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(serverSD);
        return ERROR_CODE;
    }
    recvNum(serverSD, port);

    if (*port == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(serverSD);
        return ERROR_CODE;
    }
    return 0;
}

void commandChat() {

    // notifico al server il bisogno di iniziare una chat
    int rPort, rId;
    int serverSD = createSocket(server.port);
    sendNum(serverSD, COMMAND_CHAT);

    // chiamo la requestDeviceData che assegna a rId e rPort i valori relativi al dispositivo chiamato
    // altrimenti assegna valori in base al problema
    if (requestDeviceData(serverSD, &rId, &rPort) == ERROR_CODE) {
        close(serverSD);
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
    // se entro nell'il dispositivo con cui volevo dialogare è crashato, lo notifico al server
    // che evidentemente non lo sapeva altrimenti mi avrebbe avvertito e avvio la chat offline
    if (devices[rId].sd == ERROR_CODE) {
        serverSD = createSocket(server.port);
        sendNum(serverSD, USER_OFFLINE);
        sendNum(serverSD, rId);
        sleep(1);
        handleChatServer(serverSD);
        return;
    }
    // invio al dispositivo con cui avvio la chat il mio id
    sendNum(devices[rId].sd, thisDev.id);
    sendNum(devices[rId].sd, 0);
    FD_SET(devices[rId].sd, &master);
    if (devices[rId].sd > fdmax) { fdmax = devices[rId].sd; }
    handleChat(rId, false);
    // segnalo al server che sono tornato disponibile ad avviare chat
    serverSD = createSocket(server.port);
    sendNum(serverSD, COMMAND_NOT_BUSY);
    sendNum(serverSD, thisDev.id);
    close(serverSD);
}

// funzione che permette di leggere i messaggi pendenti ricevuti dal thisDev
void commandShow() {
    char username[1024], type[5] = { "txt" };;
    int ret, serverSD = createSocket(server.port);
    printf("[SHOW] Insert <username>\n");
    // indico di quale dispositivo voglio leggere i messaggi inviatimi
    scanf("%s", username);
    sendNum(serverSD, COMMAND_SHOW);
    sendNum(serverSD, thisDev.id);
    sendMsg(serverSD, username);
    recvNum(serverSD, &ret);

    if (ret == ERROR_CODE) {
        printf("[SHOW] Nothing to show\n");
        close(serverSD);
        return;
    }

    recvFile(serverSD, type);
    FILE* fp = fopen("recv.txt", "r");
    // leggo e stampo tutto il file dei messaggi pendenti
    if (fp) {
        char buff[BUFFER_SIZE];
        while (fgets(buff, BUFFER_SIZE, fp) != NULL)
            printf("%s", buff);
        fclose(fp);
    }

    else {
        printf("[SHOW] Error, nothing to read\n");
    }
    printf("[SHOW] Completed\n");
    close(serverSD);
}

// comando inserito per andare offline
void commandOut() {
    int serverSD = createSocket(server.port);
    // se il server è offline mi salvo l'istante di logout
    if (serverSD != ERROR_CODE) {
        printf("Il server è offline, salvo il mio istante di logout in un file\n");
    }
    else {
        sendNum(serverSD, COMMAND_OUT);
        sendNum(serverSD, thisDev.id);
    }
    close(serverSD);
    thisDev.logged = false;
    printf("********************* DEVICE %d OFFLINE ********************\n", thisDev.id);
    close(listeningSocket);
    FD_CLR(listeningSocket, &master);
}

void readCommand() {
    char command[20];

    if (!thisDev.logged) {

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
        if (!strcmp(command, "show")) {
            commandShow();
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
    thisDev.logged = false;
    for (i = 0; i < MAX_DEVICES; i++) {
        devices[i].sd = -1;
    }
    fdtInit();
    FD_SET(listeningSocket, &master);
    fdmax = listeningSocket;

    while (1) {
        if (!thisDev.logged)
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
                else if (i == listeningSocket) {   //handle request (server or other device)
                    handleRequest(false);
                }
            }
        }
    }
}