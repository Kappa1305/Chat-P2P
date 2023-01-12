
#include <arpa/inet.h>
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


fd_set master;          //main set: managed with macro 
fd_set read_fds;        //read set: managed from select()
int fdmax;


struct stat st = { 0 };

struct device {
    char* username;
    char* password;
    int port;
    time_t timestampLogin;
    time_t timestampLogout;
    bool busy;
    int id;
    int chatSD; // utilizzato per le chat tra dev e dev offline
    int rId; // id dell'utente a cui sta inviando messaggi offline
}devices[MAX_DEVICES];

int nDev;

int thisPort; // porta del server

struct pending {
    int num;
    time_t lastMsgTimestamp;
}pend[MAX_DEVICES][MAX_DEVICES];

/*----------------------------------------------------------------------*\
|                  ***     FUNZIONI GENERALI     ***                     |
\*----------------------------------------------------------------------*/

void fdtInit() {
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);

    fdmax = 0;
    printf("[FDT] fdt_init: set init done!\n");
}

void timestampTranslate(time_t timestamp, char* buf) {
    struct tm* ts;
    ts = localtime(&timestamp);
    strftime(buf, sizeof(buf), "%X", ts);
}

// avvio del server
void restoreServer() {
    int i;
    char buff[1024];
    FILE* restoreFile = fopen("restoreServer.txt", "r");

    if (!restoreFile) {
        nDev = 0;
        printf("<RESTORE> First access, nothing to restore\n");
        return;
    }
    // primo numero nel file rappresenta il numero di dispositivi registrati
    fscanf(restoreFile, "%d\n", &nDev);
    for (i = 0; i < nDev; i++) {
        fgets(buff, sizeof(buff), restoreFile);
        struct device* d = &devices[i];
        //use strtok() to get buffer values 

        char* b = strtok(buff, " ");
        d->id = atoi(b);                        //id

        b = strtok(NULL, " ");
        d->username = malloc(sizeof(b));        //username
        strcpy(d->username, b);


        b = strtok(NULL, " ");
        d->password = malloc(sizeof(b));        //password
        strcpy(d->password, b);

        b = strtok(buff, " ");
        d->timestampLogin = atoi(b);
        /*b = strtok(NULL, " ");
        strcpy(d->time_login, b);               //time_login
        b = strtok(NULL, " ");
        d->port = atoi(b);                      //port
        b = strtok(NULL, " ");
        d->pend_dev_before_logout = atoi(b);    //pend_dev_before_logout
        b = strtok(NULL, " ");
        d->pend_dev = atoi(b);                  //pend_dev
        */
    }

    fclose(restoreFile);
}

void handleDevCrash(int sd) {

}

// prende in ingresso l'username di un dispositivo e restituisce
// il suo id
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

// chiamata al momento della registrazione per aggiungere un nuovo dispositivo
void devAdd(char username[1024], char password[1024]) {
    struct device* dev = &devices[nDev];
    dev->id = nDev;
    dev->busy = false;
    dev->username = malloc(sizeof(username) + 1);
    dev->password = malloc(sizeof(password) + 1);
    strcpy(dev->username, username);
    strcpy(dev->password, password);
    dev->timestampLogout = 0;
    dev->timestampLogin = 0;
    nDev++;
}


// chiamata al momento del login per aggiornare la struttura dati del dev
void devUpdate(int id, int port) {
    struct device* dev = &devices[id];
    dev->busy = false;
    dev->port = port;
    dev->timestampLogin = time(NULL);
    // dev->timestampLogout = 0;
}

// chiamata al momento della registrazione per controllare che l'username
// non sia già in uso
int signupControl(char username[1024]) {
    int i;
    struct device* dev;
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (!strcmp(dev->username, username))
            return 1;
    }
    return 0;
}

// controll che i dati inseriti in fase di login siano corretti
int loginCheck(char username[1024], char password[1024]) {
    int i;
    struct device* dev;
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (!strcmp(dev->username, username)) {
            if (!strcmp(dev->password, password)) {
                return i;
            }
            else {
                printf("<LOGIN> Wrong password\n");
                return -1;
            }
        }
    }
    // se sono uscito dal for l'username non è stato trovato
    printf("<LOGIN> Username not found\n");
    return -1;
}

int creaSocket() {
    int ret, sd;
    struct sockaddr_in my_addr;
    // Creazione socket 
    sd = socket(AF_INET, SOCK_STREAM, 0);
    if (sd == -1) {
        perror("Something went wrong during bind()\n");
        exit(-1);
    }
    if (setsockopt(sd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
        perror("Something went wrong during setsockopt(..,SO_REUSEADDR,..)\n");
    }
    // Creazione indirizzo di bind 
    memset(&my_addr, 0, sizeof(my_addr)); // Pulizia 
    my_addr.sin_family = AF_INET;
    my_addr.sin_port = htons(thisPort);
    my_addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(sd, (struct sockaddr*)&my_addr, sizeof(my_addr));
    if (ret == -1) {
        perror("Something went wrong during bind()\n");
        exit(-1);
    }
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
        if (dev->timestampLogin > dev->timestampLogout)
            // username     port    timestampLogin
            printf("%s\t\t%u\t%d\n", dev->username, dev->port, dev->timestampLogin);
    }
}

void commandHelp() {
    printf("Choose operation:\n"
        "- list -> print a list of the users online\n"
        "- esc -> turn off the server\n");
}

void commandEsc() {
    int i;
    FILE* restoreFile = fopen("restoreServer.txt", "w");
    struct device* dev;
    fprintf(restoreFile, "%d\n", nDev);
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        //copy network status in a file
        fprintf(restoreFile, "%d %s %s %d\n",
            dev->id, dev->username,
            dev->password, dev->timestampLogin
            /* dev->time_login,
             dev->port,
            d->pend_dev_before_logout, d->pend_dev*/
        );
    }
    fclose(restoreFile);
    exit(0);
}
/*----------------------------------------------------------------------*\
|                     ***     COMANDI CLIENT     ***                     |
\*----------------------------------------------------------------------*/
void login(sd) {
    char username[1024];
    char password[1024];
    int id, port;
    if (recvMsg(sd, username) == ERROR_CODE) {
        printf("<ERROR> [IN] Something wrong happened...\n");
        close(sd);
        return;
    }
    if (recvMsg(sd, password) == ERROR_CODE) {
        printf("<ERROR> [IN] Something wrong happened...\n");
        close(sd);
        return;
    }
    port = recvNum(sd);
    if (port == ERROR_CODE) {
        printf("<ERROR> [IN] Something wrong happened...\n");
        close(sd);
        return;
    }
    id = loginCheck(username, password);
    if (id == -1) {
        printf("[LOGIN] Fail\n");
        sendNum(sd, ERROR_CODE);
    }
    else {
        printf("[LOGIN] Success\n");
        id = findDevice(username);
        /*if (id == -1) {
            id = deviceSetup(username);
        }*/
        devUpdate(id, port);
        sendNum(sd, id);
    }
}

void signup(sd) {
    char username[1024];
    char password[1024];

    if (recvMsg(sd, username) == ERROR_CODE) {
        printf("<ERROR> [SIGNUP] Something wrong happened...\n");
        close(sd);
        return;
    }
    if (recvMsg(sd, password) == ERROR_CODE) {
        printf("<ERROR> [SIGNUP] Something wrong happened...\n");
        close(sd);
        return;
    }
    if (nDev >= MAX_DEVICES || signupControl(username)) {
        sendNum(sd, 1);
    }
    else {
        devAdd(username, password);
        sendNum(sd, 0);
    }
}

void hanging(int sd) {
    struct tm ts;
    char emptyLine[] = "\n";
    int i, id = recvNum(sd);
    if (id == ERROR_CODE) {
        printf("<ERROR> Something went wrong happened\n");
        close(sd);
        //handleDevCrash(sd);
        return;
    }
    struct device* dev = &devices[id];
    char buf[80];
    for (i = 0; i < MAX_DEVICES; i++) {
        printf("%d\n", pend[i][id].num);
        if (pend[i][id].num != 0) {
            sendMsg(sd, devices[i].username);
            sendNum(sd, pend[i][id].num);
            ts = *localtime(&pend[i][id].lastMsgTimestamp);
            strftime(buf, sizeof(buf), "%a %Y-%m-%d %H:%M:%S %Z", &ts);
            sendMsg(sd, buf);
        }
    }
    sendMsg(sd, emptyLine);
    printf("<HANGING> All hanging devices sent\n");
}
void prepareChatOffline(int sd, int rId) {
    int sId;
    FD_SET(sd, &master);
    if (sd > fdmax) { fdmax = sd; }
    sId = recvNum(sd);
    if (sId == ERROR_CODE) {
        printf("<ERROR> Something wrong happened...\n");
        close(sd);
        //handleDevCrash(sd);
        return;
    }
    devices[sId].chatSD = sd;
    devices[sId].rId = rId;
    devices[sId].busy = 1;
}

void handleChatRequest(sd) {
    char username[1024];
    int rId;
    struct device* dev;
    if (recvMsg(sd, username) == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(sd);
        //handleDevCrash(sd);
        return;
    }
    rId = findDevice(username);
    if (rId == -1) {
        printf("[CHAT] User '%s' not found\n", username);
        sendNum(sd, USER_NOT_FOUND);
        sendNum(sd, USER_NOT_FOUND);
        return;
    }
    dev = &devices[rId];
    sendNum(sd, rId);

    if (dev->timestampLogin <= dev->timestampLogout) {
        sendNum(sd, USER_OFFLINE);
        printf("[CHAT] %s is offline, preparing for offline chat\n", username);
        prepareChatOffline(sd, rId);
        return;
    }
    if (dev->busy) {
        sendNum(sd, USER_BUSY);
        printf("[CHAT] %s is busy, preparing for offline chat\n", username);
        prepareChatOffline(sd, rId);
        return;
    }
    printf("[CHAT] %s is online, they'll handle chat themeself\n", username);
    sendNum(sd, dev->port);
    return;
}


int out(sd) {
    int id = recvNum(sd);
    if (id == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(sd);
        //handleDevCrash(sd);
        return ERROR_CODE;
    }
    struct device* dev = &devices[id];
    dev->timestampLogout = time(NULL);
    printf("[OUT] Dev %d is now offline\n", id);
    return id;
}

void usernameOnline(sd) {
    int i;
    struct device* dev;
    char emptyLine[] = "\n";
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (dev->timestampLogin > dev->timestampLogout) {
            sendMsg(sd, dev->username);
        }
    }
    // il dispotivo sa da protocollo che quando ottiene una riga vuota significa
    // che non ci sono ulteriori dispositivi online oltre a quelli già inviati
    sendMsg(sd, emptyLine);
    printf("[OUT] All online users were sent\n");
}

void recvCommand(int sd) {
    int command;
    int id; // usato per la busy e not busy
    command = recvNum(sd);
    if (command == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(sd);
        //handleDevCrash(sd);
        return;
    }
    switch (command) {
    case COMMAND_SIGNUP:
        printf("Command received : [signup]\n");
        signup(sd);
        break;
    case COMMAND_IN:
        printf("Command received : [in]\n");
        login(sd);
        break;
    case COMMAND_HANGING:
        printf("Command received : [hanging]\n");
        hanging(sd);
        break;
    case COMMAND_CHAT:
        printf("Command received : [chat]\n");
        handleChatRequest(sd);
        break;
    case COMMAND_OUT:
        printf("Command received : [out]\n");
        out(sd);
        break;
    case COMMAND_DEVICE_DATA: // USERNAME
        printf("Command received : [device data]\n");
        usernameOnline(sd);
        break;
         // qualcuno voleva iniziare una chat con un dispositivo ma non risponde, considero la destinazione
         // offline e inizio una chat con il chiamante
    case USER_OFFLINE:
        printf("Command received : [device crashed]\n");
        id = out(sd); // la out restituisce l'id del dispositivo disconnesso
        if(id == ERROR_CODE)
            return;
        prepareChatOffline(sd, id);
        break;
    case COMMAND_BUSY:
        id = recvNum(sd);
        if (id == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(sd);
            //handleDevCrash(sd);
            return;
        }
        devices[id].busy = true;
        printf("Command received : %d is busy\n", id);
        break;
    case COMMAND_NOT_BUSY:
        id = recvNum(sd);
        if (id == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(sd);
            //handleDevCrash(sd);
            return;
        }
        devices[id].busy = false;
        printf("Command received : %d is no longer busy\n", id);
        break;
    default:
        printf("Unknown command\n");
    }
}

void readCommand() {
    char command[20];
    scanf("%s", command);
    if (!strcmp(command, "list")) {
        commandList();
        return;
    }
    if (!strcmp(command, "help")) {
        commandHelp();
        return;
    }
    if (!strcmp(command, "esc")) {
        commandEsc();
        return;
    }
    printf("[READ COMMAND] Invalid operation\n");
}

void handleChat(int sd) {

    char path[1024], filename[1024];
    char msg[1024];
    int sId, rId, i;
    struct device* dev;
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (dev->chatSD == sd) {
            sId = i;
            break;
        }
    }
    rId = dev->rId;
    if (recvMsg(sd, msg) == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        FD_CLR(dev->chatSD, &master);
        close(dev->chatSD);
        close(sd);
        //handleDevCrash(sd);
        return;
    }
    if (!strncmp(msg, "\\q", 2)) {
        FD_CLR(dev->chatSD, &master);
        close(dev->chatSD);
        devices[sId].busy = 0;
        return;
    }
    sprintf(path, "./pending_messages");
    if (stat(path, &st) == -1)
        mkdir(path, 0700);

    //subdirectory for receiver
    sprintf(path, "./pending_messages/device_%d", rId);
    if (stat(path, &st) == -1)
        mkdir(path, 0700);

    sprintf(filename, "%s/from_%d.txt", path, sId);
    printf("[CHAT] CreateD file to save messages:\n\t%s\n", filename);

    FILE* fp;
    if ((fp = fopen(filename, "a")) == NULL) {
        perror("[CHAT] Error: fopen()");
        exit(-1);
    }
    pend[sId][rId].num++;
    pend[sId][rId].lastMsgTimestamp = time(NULL);
    //handle time for message



    //copy messages in a file
    fprintf(fp, msg);
    //printf("\ts_id: %d\tr_id: %d\tn_msgs:  %d\n", s_id, r_id, pending_messages[s_id][r_id].num);
    fclose(fp);

}

/*----------------------------------------------------------------------*\
|                         ***     MAIN     ***                           |
\*----------------------------------------------------------------------*/


int main(int argc, char* argv[]) {
    struct sockaddr_in cl_addr;
    int ret, sd, new_sd, len, i;
    struct device* dev;
    system("clear");
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
    restoreServer();
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
                if (i == 0) {                      //keyboard
                    readCommand();
                    continue;
                }
                if (i == sd) {       //device request  
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
                    continue;
                }
                // un utente sta mandando un messaggio a un dev offline
                handleChat(i);
            }
        }
    }

}