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


/*--**********************************--*\
|        *** STRUTTURE DATI ***          |
\*--**********************************--*/

struct stat st = { 0 };

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


fd_set master;          //main set: gestita con le macro
fd_set read_fds;        //read set: gestita dalla select()
int fdmax;


/*--**********************************--*\
|    *** DICHIARAZIONE FUNZIONI ***      |
\*--**********************************--*/
// necessario dichiararle anticipatamente per come è strutturato il codice
// per esempio la handle request deve chiamare in un caso la handleChat() quindi dovrebbe essere definita successivamente ad essa
// ma la stessa handleChat() deve chiamare la handleRequest e perciò la handlerequest dovrebbe essere definita prima di essa
// dichiararle in anticipo ci permette di risolvere il problema

void handleRequest(bool inChat);
int requestDeviceData(int serverSD, char* username, int* id, int* port);

/*--**********************************--*\
|       *** FUNZIONI GENERALI ***        |
\*--**********************************--*/

// inizializza i set per la select
void fdtInit() {
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);

    fdmax = 0;

    printf("[FDT INIT] Done\n");
}

// crea i socket su cui i device ascolterranno
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

// funzione che ci permette di controllare se un username è nella rubrica del chiamante
bool rubricControl(char* username) {
    char filename[1024], buff[1024];
    sprintf(filename, "rubric/%s.txt", thisDev.username);
    FILE* fp = fopen(filename, "r");
    if (fp)
    {
        // leggo tutti gli username nella rubrica cercando una corrispondenza
        while (fgets(buff, sizeof(buff), fp)) {
            if (!strncmp(buff, username, strlen(username))) {
                printf("[ADDRESS BOOK] Username found\n");
                return true;
            }
        }
    }
    // se entro nell'else la fopen ha restituito NULL ciò significa che non esiste il file di rubrica
    else
        printf("[ADDRESS BOOK] Rubric not found\n");
    // se arrivo qua l'uesaname non è stato trovato (incluso caso in cui manchi la rubrca)
    printf("[ADDRESS BOOK] Username not found\n");
    return false;
}


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

//  chiamata dopo l'invio di un comando al server di richiesta di dati riguardo un username, in caso
// di problemi sull'utente il tipo di errore si ricava dall'id o porta
int requestDeviceData(int serverSD, char* username, int* id, int* port) {
    //printf("[CHAT] Insert <username>\n");
    //scanf("%s", username);
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


/*--**********************************--*\
|        ***  COMANDI CHAT  ***          |
\*--**********************************--*/

// risponde al comando '\h' stampando informazioni riguardo gli altri comandi 
void chatCommandHelp() {
    printf(
        "[HELP] type a message + ENTER to send it\n"
        "COMMANDS:\n"
        "\\a [user]: add [user] to chat\n"
        "\\s [file] : share [file] to users in chat\n"
        "\\q : quit chat\n");
}

// risponde al comando '\q' facendo uscire l'user chiamante dalla chat in corso
void chatCommandQuit() {
    int i;
    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            // informo tutti gli altri dispositivi qual è l'id del dispositivo che è uscito dalla chat
            sendNum(devices[i].sd, thisDev.id);
            // tolgo i socket degli altri dispositivi da quelli che ascolto (sono dentro un for, li tolgo tutti)
            FD_CLR(devices[i].sd, &master);
            sleep(1);
            close(devices[i].sd);
            // -1 indica che non è un dispositivo che sto ascoltando
            devices[i].sd = -1;
        }
    }
    nDevChat = 0;
    system("clear");
    return;
}

// [ADD--

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

// risponde al comando '\a' aggiungendo un dispositivo alla chat se possibile 
void chatCommandAdd(sd) {

    int rId, rPort, i, serverSD;
    char username[1024];

    printf("[CHAT] Insert <username> to add\n");
    scanf("%s", username);
    // controllo di non aggiungere lo stesso dispositivo che chiama
    if (!strcmp(username, thisDev.username)) {
        printf("<ERROR> You can't chat with yourself\n");
        notifyAddError();
        return;
    }
    // controllo che lo username sia nella rubrica
    if (!rubricControl(username)) {
        sleep(1);
        notifyAddError();
        return;
    }

    serverSD = createSocket(server.port);
    // controllo che il server sia ancora online
    // se il server è offline invio a tutti un messaggio di errore
    // che indica che la add non può essere completata
    if (serverSD == ERROR_CODE) {
        notifyAddError();
        return;
    }
    // dispositivo chiede al server id e porta del dispositivo da aggiungere
    // inviandogli l'username
    sendNum(serverSD, COMMAND_ADD);
    // la funzione requestDeviceData assegna i valori a rId e rPort, in caso di problemi con l'user assegna i valori
    // a rId e rPort, la funzione restituisce ERROR_CODE se c'è stato un errore sulle recv (server andato offline)
    if (requestDeviceData(serverSD, username, &rId, &rPort) == ERROR_CODE) {
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

    // crea un socket col dispositivo aggiunto e lo informa del proprio id
    devices[rId].sd = createSocket(rPort);
    sendNum(devices[rId].sd, thisDev.id);

    // questa send notifica al dispositivo che sta per entrare in una chat di gruppo
    // ciò implica che non dovrà stampare la history della chat
    sendNum(devices[rId].sd, GROUP_CHAT);
    // aggiunge il socket del nuovo dispositivo a quelli di ascolto
    FD_SET(devices[rId].sd, &master);
    if (devices[rId].sd > fdmax) { fdmax = devices[rId].sd; }

    nDevChat++;

    return;
}

// --ADD]

// risponde al comando '\u' stampando tutti gli username dei dispositivi online  
void chatCommandUsername() {
    char username[1024];
    int serverSD = createSocket(server.port);
    // se entra nell'if il server è offline
    if (serverSD == ERROR_CODE) {
        return;
    }
    sendNum(serverSD, COMMAND_DEVICE_DATA);
    printf("[USERS ONLINE]\n");
    // il server invia la lista di utenti online uno per uno, il device non deve far altro che ricevere e stampare
    while (1) {
        if (recvMsg(serverSD, username) == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(serverSD);
            return;
        }
        // il carattere di fine stringa indica che la lista di username è terminata (così è deciso da protocollo)
        if (username[0] == '\n')
            break;
        printf("%s\n", username);
    }
}

// [SHARE --

// risponde al comando '\s' inviando un file a tutti gli user in chat
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

    // tutto bene, invio a tutti un ok_code, il tipo del file ed il file
    char* name = strtok(msg, ".");
    char* type = strtok(NULL, ".");
    printf("[SHARE] sending %s type %s...\n", name, type);

    for (i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].sd != -1) {
            // notifico che tutto è andato a buon fine e si può dunque effettuare il trasferimento
            sendNum(devices[i].sd, OK_CODE);
            sendMsg(devices[i].sd, type);
            // dico che tipo di file debbano aspettarsi

            ret = sendFile(devices[i].sd, fp);
            if (ret == ERROR_CODE) {
                printf("<ERROR> Something wrong happened\n");
                return;
            }
            sleep(1);
            // se non inizializzassi il puntatore a carattere nel file la prossima lettura inizierebbe dall'ultimo carattere del file
            // => leggerebbe 0 bytes
            fseek(fp, 0, SEEK_SET);
        }
    }

    printf("[SHARE] File shared!\n");
}


// funzione chiamata dal dispositivo che deve ricevere un file in chat
void chatCommandShareRecv(sd) {
    printf("[SHARE RECV] Other device is sending you a file: wait...\n");
    int ret;
    // aspetta di ricevere OK_CODE per sapere che il dispositivo che sta inviando non ha avuto problemi
    recvNum(sd, &ret);
    if (ret == SHARE_ERROR) {
        printf("[SHARE RECV] File transfer failed: sender error!\n");
        return;
    }

    // ottiene il tipo di file
    char type[10];
    if (recvMsg(sd, type) == ERROR_CODE) {
        printf("[SHARE RECV] File transfer failed: sender error!\n");
        return;
    }

    printf("[device] receiving %s file...\n", type);
    recvFile(sd, type);
    struct stat st;
    // recv.txt è il file in cui la recvFile salva il file
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

// --SHARE]

// funzione chiamata per mostrare la history della chat passata con l'altro utente
void readChat(int id) {
    char filename[WORD_SIZE];
    sprintf(filename, "chat_device_%d/chat_with_%d.txt", thisDev.id, id);
    FILE* fp = fopen(filename, "r");
    if (fp) {
        // legge ogni riga del file e la stampa
        char buff[BUFFER_SIZE];
        while (fgets(buff, BUFFER_SIZE, fp) != NULL)
            printf("%s", buff);
        fclose(fp);
    }
}


/*--**********************************--*\
|    ***  FUNZIONI DI SUPPORTO     ***   |
|      ***      ALLA CHAT        ***     |
\*--**********************************--*/

// aggiunge informazioni al messaggio inviato
// formato: username [Day yyyy-mm-dd hh:mm:ss CET] *msg*
void addMsgData(char* msg) {
    char tmp[1024];
    strcpy(tmp, msg);
    struct tm ts;
    time_t msgTime;
    char buf[1024];
    // assegna il valore corrente di ts
    time(&msgTime);
    ts = *localtime(&msgTime);
    // traduce il timestamp nel formato desiderato
    strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
    // aggiungo effettivamente le info al messaggio
    sprintf(msg, "%s [%s]: %s", thisDev.username, buf, tmp);
}

// funzione principale della chat
// l'id è quello del dispositivo con cui il dispositivo entra in chat, groupchat indica se è una chat di gruppo o no
void handleChat(int id, bool groupChat) {
    char msg[1024];
    int i, j, code, rId, rPort;
    // valore che verrà incrementato ogni volta che qualcuno si aggiunge alla chat, utile per sapere quando si è
    // rimasti gli unici in chat, non si può utilizzare per controllare se chiamare la readChat perchè se qualcuno 
    // mi ha aggiunto a una chat già esistente gli altri utenti in chat mi aggiungeranno solo dopo la select
    nDevChat = 1;
    // notifico al server che sono attualmente busy, invierò alla fine della chat un command_not_busy
    int serverSD = createSocket(server.port);
    sendNum(serverSD, COMMAND_BUSY);
    sendNum(serverSD, thisDev.id);
    close(serverSD);
    system("clear");
    if (!groupChat)
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
                    // il codice indica se è un comando o un semplica messaggio
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
                                if (sendMsg(devices[j].sd, msg) == ERROR_CODE) {
                                    devices[j].sd = -1;
                                }
                            }
                        }
                        // se non è una chat di gruppo aggiungo il messaggio inviato alla history
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
                        break;

                    case SHARE_CODE:
                        chatCommandShare();
                        break;
                    }
                }
                // qualcuno vuole aggiungersi alla chat
                else if (i == listeningSocket) {
                    handleRequest(true);
                    nDevChat++;
                }
                // ricevuto un messaggio da qualcun'altro
                else if (i != listeningSocket) {
                    // se error_code qualcuno è crashato 
                    if (recvNum(i, &code) == ERROR_CODE) {
                        // la handleDevCrash restituisce 1 se devo terminare la chat
                        if (handleDevCrash(i)) {
                            return;
                        }
                        continue;
                    }

                    switch (code) {
                        // ho ricevuto un messaggio
                    case OK_CODE:
                        if (recvMsg(i, msg) == ERROR_CODE) {
                            // la handleDevCrash restituisce 1 se devo terminare la chat -> sono rimasto solo in chat
                            if (handleDevCrash(i)) {
                                return;
                            }
                            continue;
                        }
                        printf("%s", msg);
                        // se non è una chat di gruppo aggiungo il messaggio inviato alla history
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
                        // qualcuno in chat ha chiamato la HELP, non mi interessa
                        break;

                    case ADD_CODE:
                        // ricevo l'id del dispositivo da aggiungere in chat
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
                        // ricevo la porta dell'utente in chat
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

// chiamata quando l'utente con cui volevo iniziare la chat è offline, invierò i messaggi al server che li renderà pendenti
// per il ricevitore
void handleChatServer(int serverSD) {
    char msg[1024];
    int code;
    system("clear");
    printf("[CHAT] You are now chatting with server\n");
    // informo il server del mio id, così che possa dire al ricevitore chi gli ha scritto ed informarmi
    // quando li leggerà
    sendNum(serverSD, thisDev.id);
    while (1) {
        do {
            fgets(msg, 1024, stdin);
        } while (msg[0] == '\n');
        code = checkChatCommand(msg);
        if (code == OK_CODE)
            addMsgData(msg);
        // invio il comando solo se è consentito in una chat col server
        if (code == OK_CODE || code == QUIT_CODE) {
            if (sendMsg(serverSD, msg) == ERROR_CODE) {
                printf("<ERROR> Server might be offline\n");
                close(serverSD);
                return;
            }
        }
        switch (code) {
        case OK_CODE:
            break;
        case QUIT_CODE:
            close(serverSD);
            return;
        default:
            printf("[CHAT] Invalid command\n");
        }
    }
}

// chiamata dal dispositivo aggiunto alla chat quando un dispositivo 
// (non quello che ha chiamato la \a) desidera connettersi oppure quando 
// qualcuno vuole iniziare una chat con me oppure quando 
// il server vuole notificare che qualcuno ha effettuato la show su dei 
// messaggi pendenti del thisDev
void handleRequest(bool inChat) {
    int chatSD, sId; // sender id
    int groupChat;
    struct sockaddr_in s_addr;
    socklen_t addrlen = sizeof(s_addr);
    chatSD = accept(listeningSocket, (struct sockaddr*)&s_addr, &addrlen);
    // ricevo l'id del dispositivo che mi ha aggiunto, se è il server mi invia COMMAND_SHOW
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
    FD_SET(devices[sId].sd, &master);
    if (devices[sId].sd > fdmax) { fdmax = devices[sId].sd; }
    // se la chat deve ancora iniziare chiamo la handleChat(), se è qualcuno che mi ha aggiunto a una
    // chat già esistente sono già dentro la handle chat
    if (!inChat) {
        recvNum(devices[sId].sd, &groupChat);
        handleChat(sId, groupChat);
        close(devices[sId].sd);
    }
}

/*--**********************************--*\
|        ***     COMANDI     ***         |
\*--**********************************--*/

// funzione chiamata in risposta al comando IN
void commandIn() {
    int ret;
    int serverSD;
    char username[1024];
    char password[1024];
    printf("[IN] Insert <server port> <username> <password>\n");

    scanf("%d", &server.port);
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
    // assegno a ret il valore dell'id del dispositivo che ha fatto il login
    recvNum(serverSD, &ret);
    if (ret == ERROR_CODE) {
        printf("[IN] Failed\n");
    }

    else {
        printf("[IN] Success\n");
        createListeningSocket();
        printf("********************* DEVICE %d ONLINE ********************\n", ret);
        thisDev.id = ret;
        thisDev.logged = true;
        thisDev.username = malloc(sizeof(username));
        strcpy(thisDev.username, username);
        // creo se non esiste la cartella che contiene la hitory delle chat del dispositivo
        struct stat st = { 0 };
        char dir_path[15];
        sprintf(dir_path, "./chat_device_%d", thisDev.id);
        if (stat(dir_path, &st) == -1)
            mkdir(dir_path, 0700);

        // se esiste questo file significa che l'ultima volta che il dispositivo ha effettuato il logout il server
        // era offline, nel file è salvato il timestamp in cui è avvenuto il logout e viene notificato al server
        char filename[1024];
        sprintf(filename, "saved_logout/logout_dev_%d.txt", thisDev.id);
        FILE* fp = fopen(filename, "r");
        if (fp) {
            printf("[LOG REGISTER] Notifying server over my last logout TS...\n");
            sendNum(serverSD, NOTIFY_LOGOUT_TS);
            sendFile(serverSD, fp);
            fclose(fp);
            // il file viene eliminato ad indicare che non ci sono timestamp di logout da notificare
            remove(filename);
            close(serverSD);
            printf("[LOG REGISTER] Notifying completed\n");
        }
        else
            // notifico al server che non ci sono timestamp di logout da notificare
            sendNum(serverSD, OK_CODE);
    }
}

// funzione chiamata in risposta al comando SIGNUP
void commandSignup() {
    char username[1024];
    char password[1024];
    int serverSD, ret;

    printf("[SIGNUP] Insert <server port> <username> <password>\n");

    scanf("%d", &server.port);
    scanf("%s", username);
    scanf("%s", password);
    serverSD = createSocket(server.port);
    if (serverSD == ERROR_CODE) {
        return;
    }
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

// funzione chiamata in risposta al comando HANGING
// stampa tutti gli username di tutti i dispositivi che hanno lasciato messaggi pendenti al dispositivo chiamante
// associato al numero di messaggi pendenti e alla data dell'ultimo messaggio pendente
void commandHanging() {
    char username[1024];
    int numPending;
    char timestamp[80];
    int serverSD = createSocket(server.port);
    sleep(1);
    if (serverSD == ERROR_CODE)
        return;

    sendNum(serverSD, COMMAND_HANGING);
    sendNum(serverSD, thisDev.id);

    // resta nel ciclo fino a che c'è qualcosa da stampare, quando ricevi come username '\n' significa che è finita
    // la lista
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


// funzione chiamata in risposta al comando CHAT
void commandChat() {
    int rPort, rId;
    char username[1024];
    printf("[CHAT] Insert <username> to chat with\n");
    scanf("%s", username);
    if (!strcmp(username, thisDev.username)) {
        printf("<ERROR> You can't chat with yourself\n");
        return;
    }
    // controllo che l'utente con cui iniziare una chat appartenga alla rubrica del chiamante
    if (!rubricControl(username)) {
        return;
    }
    int serverSD = createSocket(server.port);
    if (serverSD == ERROR_CODE) {
        return;
    }
    // notifico al server il bisogno di iniziare una chat
    sendNum(serverSD, COMMAND_CHAT);

    // chiamo la requestDeviceData che assegna a rId e rPort i valori relativi al dispositivo chiamato
    // altrimenti assegna valori in base al problema
    if (requestDeviceData(serverSD, username, &rId, &rPort) == ERROR_CODE) {
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
    // se entro nell'if il dispositivo con cui volevo dialogare è crashato, lo notifico al server
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

// funzione chiamata in risposta al comando SHOW
// permette di visualizzare i messaggi pendenti inviati dal dispositivo indicato
void commandShow() {
    char username[1024], type[5] = { "txt" };;
    int ret, serverSD = createSocket(server.port);
    if (serverSD == ERROR_CODE) {
        return;
    }
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
    // i messaggi pendenti vengono inviati in un file, la funzione si limita a ricevere e stampare riga per riga
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

// funzione chiamata in risposta al comando OUT
// permette al dispositivo di andare offline
void commandOut() {
    int serverSD = createSocket(server.port);
    // se il server è offline mi salvo l'istante di logout
    if (serverSD == ERROR_CODE) {
        printf("[OUT] Server offline... Saving logout timestamp...\n");
        struct stat st = { 0 };
        char dir_path[15];
        // se non esiste la cartella che gestisce i logout passati la creo (vale per tutti i dispositivi)
        sprintf(dir_path, "./saved_logout");
        if (stat(dir_path, &st) == -1)
            mkdir(dir_path, 0700);
        char filename[WORD_SIZE];
        // crea un file riguardante il dispositivo specifico 
        sprintf(filename, "%s/logout_dev_%d.txt", dir_path, thisDev.id);
        FILE* fp = fopen(filename, "w");
        if (fp) {
            fprintf(fp, "%u", (unsigned)time(NULL));
            fclose(fp);
            printf("[OUT] Saving completed\n");
        }

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
    // se il dispositivo non ha ancora effettuato il login i comandi disponibili sono 'in' e 'signup'
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
    // se il dispositivo ha effettuato il login i comandi disponibili sono 'hanging', 'show', 'chat', e 'out'
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
    // pulisce il prompt
    system("clear");
    // l'utente passa come agomento la porta a cui risponde il dispositivo
    thisDev.port = atoi(argv[1]);
    thisDev.logged = false;

    for (i = 0; i < MAX_DEVICES; i++) {
        devices[i].sd = -1;
    }

    fdtInit();
    FD_SET(listeningSocket, &master);
    fdmax = listeningSocket;

    while (1) {
        // due print diverse se il dispositivo ha già effettuato il login oppure non ancora
        if (!thisDev.logged)
            printf("[OPERATION] Choose operation:\n"
                "- signup <server port> <username> <password>\n"
                "- in <server port> <username> <password>\n");
        else
            printf("[OPERATION] Choose operation:\n"
                "- hanging\n"
                "- show <username>\n"
                "- chat <username>\n"
                "- out\n");
        // controllo se qualche socket è pronto
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