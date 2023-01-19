
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


/*--**********************************--*\
|        *** STRUTTURE DATI ***          |
\*--**********************************--*/

fd_set master;          //main set: gestita con le macro
fd_set read_fds;        //read set: gestita dalla select()
int fdmax;


struct stat st = { 0 };

struct device {
    char* username;
    char* password;
    int port;
    time_t timestampLogin;
    bool online;
    bool busy;
    int id;
    int chatSD; // utilizzato per le chat tra dev e dev offline
    int rId; // id dell'utente a cui sta inviando messaggi offline
    bool notify; // a true se qualcuno ha letto i messaggi che erano pending
}devices[MAX_DEVICES];

int nDev;

int thisPort; // porta del server

struct pending {
    int num;
    time_t lastMsgTimestamp;
}pend[MAX_DEVICES][MAX_DEVICES];


/*--**********************************--*\
|       *** FUNZIONI GENERALI ***        |
\*--**********************************--*/


void fdtInit() {
    FD_ZERO(&master);
    FD_ZERO(&read_fds);
    FD_SET(0, &master);

    fdmax = 0;
    printf("[FDT] fdt_init: set init done!\n");
}

// traduce il timestamp da time_t (unsigned) a formato umano
void timestampTranslate(time_t timestamp, char* buf) {
    struct tm* ts;
    ts = localtime(&timestamp);
    strftime(buf, sizeof(buf), "%X", ts);
}

// avvio del server, controllo se esiste un file da cui recuperare informazioni precendenti
void restoreServer() {
    int i, j;
    char buff[1024], * eptr; // eptr necessario per la conversione da stringa a unsigned di ts
    char* b;
    FILE* fp = fopen("restoreServer.txt", "r");

    if (!fp) {
        nDev = 0;
        printf("<RESTORE> First access, nothing to restore\n");
        return;
    }
    // primo numero nel file rappresenta il numero di dispositivi registrati
    fscanf(fp, "%d\n", &nDev);
    for (i = 0; i < nDev; i++) {
        fgets(buff, sizeof(buff), fp);
        struct device* d = &devices[i];
        //uso strtok() per ottenere i valori dal buffer

        b = strtok(buff, " ");
        d->id = atoi(b);                        //id

        b = strtok(NULL, " ");
        d->username = malloc(sizeof(b));        //username
        strcpy(d->username, b);


        b = strtok(NULL, " ");
        d->password = malloc(sizeof(b));        //password
        strcpy(d->password, b);

        b = strtok(NULL, " ");
        d->busy = atoi(b);                       //busy

        b = strtok(NULL, " ");
        d->busy = strtoul(b, &eptr, 10);         //timestamp login


        b = strtok(NULL, " ");
        d->notify = atoi(b);                     //notify

        b = strtok(NULL, " ");
        d->port = atoi(b);                       //port
    }
    fclose(fp);

    // controllo se ho un file che indica i messaggi pendenti tra gli utenti
    fp = fopen("pending_messages.txt", "r");
    if (!fp) {
        for (i = 0; i < MAX_DEVICES; i++) {
            for (j = 0; j < MAX_DEVICES; j++) {
                pend[i][j].num = 0;
                pend[i][j].lastMsgTimestamp = 0;
                return;
            }
        }
    }
    // nel file ho un vettore tridimensionale, ogni cella della matrice contiene due info: num 
    // di messaggi pendenti e timestamp dell'ultimo messaggio
    for (i = 0; i < MAX_DEVICES; i++) {
        // la prima casella di ogni riga la faccio fuori dal for per gestire il funzionamento
        // della strtok che necessita prima del buffer da cui leggere e successivamente prende 'null'
        fgets(buff, sizeof(buff), fp);
        b = strtok(buff, " ");
        pend[i][0].num = atoi(b);
        b = strtok(NULL, " ");
        pend[i][0].lastMsgTimestamp = atoi(b);
        // prima colonna già letta, comincio a leggere dalla seconda
        for (j = 1; j < MAX_DEVICES; j++) {
            b = strtok(NULL, " ");
            pend[i][j].num = atoi(b);
            b = strtok(NULL, " ");
            pend[i][j].lastMsgTimestamp = atoi(b);
        }
    }
    fclose(fp);
}

// funzione chiamata per registrare login (login a true) e logout (login a false)
void registerLog(bool login, int id, int ts) {
    // se non esiste la cartella che gestisce il registro la creo
    char dir_path[15];
    sprintf(dir_path, "./log_register");
    if (stat(dir_path, &st) == -1)
        mkdir(dir_path, 0700);
    char filename[WORD_SIZE];
    // per ogni dispositivo ho un file che indica porta di accessso, ts login e ts logout
    // se un dispositivo esce con ctrl-c il suo ts di logout non viene registrato
    sprintf(filename, "%s/dev_%d.txt", dir_path, id);
    FILE* fp = fopen(filename, "a");
    if (fp) {
        if (login)
            fprintf(fp, "\n%d %u ", devices[id].port, ts);
        else
            fprintf(fp, "%u", ts);
        fclose(fp);
        printf("[LOG REGISTER] Log completed\n");
    }
}

// prende in ingresso l'username di un dispositivo e restituisce il suo id
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
    dev->online = false;
    dev->timestampLogin = 0;
    nDev++;
}

// chiamata per notificare a un dispositivo che aveva lasciato un messaggio pendente che il messaggio è stato letto
void notifyShow(int id) {
    sleep(1);
    int sd;
    struct device* dev = &devices[id];
    sd = createSocket(dev->port);
    if (sd == ERROR_CODE) {
        printf("[SHOW] %d is offline, i'll notify the show when it come back\n", id);
        dev->notify = true;
    }
    // si limita a inviare un numero
    sendNum(sd, COMMAND_SHOW);
    close(sd);
}

// chiamata al momento del login per aggiornare la struttura dati del dev
void devUpdate(int id, int port, int sd) {
    int code;
    char buff[BUFFER_SIZE];
    int ts;
    struct device* dev = &devices[id];
    dev->busy = false;
    dev->port = port;
    dev->timestampLogin = time(NULL);

    // dev->notify era salvata nel file di restore o nelle strutture dati del server, se è a valore a true
    // qualcuno ha letto i messaggi che il dispositivo aveva lasciato pendenti mentre il dispositivo era offline
    if (dev->notify) {
        notifyShow(id);
    }
    dev->notify = false;

    dev->online = true;
    if (recvNum(sd, &code) == ERROR_CODE) {
        printf("<ERROR> [IN] Something wrong happened...\n");
        close(sd);
        return;
    }
    // il dispositivo aveva effettuato un logout mentre il server era offline, aveva salvato il timestamp
    // e me lo notifica così che il server possa aggiornare il registro
    if (code == NOTIFY_LOGOUT_TS) {
        printf("Command received : [LOGOUT TS]\n");
        recvFile(sd, "txt");
        FILE* fp = fopen("./recv.txt", "r");
        fgets(buff, BUFFER_SIZE, fp);
        ts = atoi(buff);
        printf("mi sta passando i dati l'id %d\n", id);
        registerLog(false, id, ts);
    }
    registerLog(true, id, (unsigned)time(NULL));
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

// controlla che i dati inseriti in fase di login siano corretti
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

// funzione chiamata quando è necessario creare un socket di ascolto
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

// chiamata quando si nota che un dispositivo voleva iniziare una chat con un dispositivo non disponibile
// aggiunge il socket di comunicazione con il dispositivo tra quelli ascoltati
void prepareChatOffline(int sd, int rId) {
    int sId;
    FD_SET(sd, &master);
    if (sd > fdmax) { fdmax = sd; }
    recvNum(sd, &sId);
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


/*----------------------------------------------------------------------*\
|                     ***     COMANDI SERVER     ***                     |
\*----------------------------------------------------------------------*/

// funzione chiamata in risposta al comando LIST
// stampa l'elenco dei dispositivi online con il formato:
// username     port    timestampLogin
void commandList() {
    int i;
    struct device* dev;
    printf("username\tport\tlogin timestamp\n\n");
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (dev->online)
            printf("%s\t\t%u\t%d\n", dev->username, dev->port, (unsigned)dev->timestampLogin);
    }
}

// funzione chiamata in risposta al comando HELP
void commandHelp() {
    printf("Choose operation:\n"
        "- list -> print a list of the users online\n"
        "- esc -> turn off the server\n");
}

// funzione chiamata in risposta al comando ESC
// salva i dati utili riguardanti i dispositivi in un file
void commandEsc() {
    int i, j;
    FILE* fp = fopen("restoreServer.txt", "w");
    struct device* dev;
    fprintf(fp, "%d\n", nDev);
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        // salva i valori utili dei descrittori dei dispositivi per poterli recuperare al prossimo avvio
        fprintf(fp, "%d %s %s %u %d %d %d\n",
            dev->id, dev->username,
            dev->password, (unsigned)dev->timestampLogin,
            dev->busy, dev->notify,
            dev->port
        );
    }
    fclose(fp);
    // salvo su un file a parte i numeri di messaggi pendenti tra gli utenti e il timestamp dell'ultimo in forma matriciale
    fp = fopen("pending_messages.txt", "w");
    for (i = 0; i < MAX_DEVICES; i++) {
        for (j = 0; j < MAX_DEVICES; j++) {
            fprintf(fp, "%d %u ", pend[i][j].num, (unsigned)pend[i][j].lastMsgTimestamp);
        }
        fprintf(fp, "\n");
    }
    fclose(fp);
    exit(0);
}
/*----------------------------------------------------------------------*\
|                     ***     COMANDI CLIENT     ***                     |
\*----------------------------------------------------------------------*/

// funzione chiamata in risposta al comando ricevuto dal dispositivo LOGIN
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
    recvNum(sd, &port);
    if (port == ERROR_CODE) {
        printf("<ERROR> [IN] Something wrong happened...\n");
        close(sd);
        return;
    }
    id = loginCheck(username, password);
    if (id == -1) {
        printf("[IN] Fail\n");
        sendNum(sd, ERROR_CODE);
    }
    else {
        printf("[IN] Success\n");
        id = findDevice(username);
        /*if (id == -1) {
            id = deviceSetup(username);
        }*/
        sendNum(sd, id);
        devUpdate(id, port, sd);
    }
}

// funzione chiamata in risposta al comando ricevuto dal dispositivo SIGNUP
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

// funzione chiamata in risposta al comando ricevuto dal dispositivo HANGING
void hanging(int sd) {
    struct tm ts;
    char emptyLine[] = "\n"; // utilizzato per segnalare la fine della comunicazione
    int i, id;
    recvNum(sd, &id);
    if (id == ERROR_CODE) {
        printf("<ERROR> Something went wrong happened\n");
        close(sd);
        //handleDevCrash(sd);
        return;
    }
    char buf[80];
    // il server invia uno per uno username, numero di messaggi e timestamp dell'ultimo
    for (i = 0; i < MAX_DEVICES; i++) {
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

// funzione chiamata alla ricezione dei comandi CHAT e \a
// riceve in ingeresso un username ed invia ii valori dell'id e della porta del dispositivo relativo
int deviceData(sd) {
    char username[1024];
    int rId;
    struct device* dev;
    if (recvMsg(sd, username) == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(sd);
        //handleDevCrash(sd);
        return -2;
    }
    rId = findDevice(username);
    // in caso di username non esistente, dispotivo offline o busy viene segnalato nei valori inviati

    if (rId == -1) {
        printf("[CHAT] User '%s' not found\n", username);
        sendNum(sd, USER_NOT_FOUND);
        sendNum(sd, USER_NOT_FOUND);
        return -2;
    }
    dev = &devices[rId];
    sendNum(sd, rId);

    // se l'utente è offline o busy lo segnalo sulla rPort
    if (!dev->online) {
        sendNum(sd, USER_OFFLINE);
        return rId;
    }
    if (dev->busy) {
        sendNum(sd, USER_BUSY);

        return rId;
    }
    printf("[CHAT] %s is online, they'll handle chat themeself\n", username);
    sendNum(sd, dev->port);
    return -1;
}

// funzione chiamata in risposta al comando ricevuto dal dispositivo SHOW
// riceve l'username del dispositivo che ha inviato i messaggi e l'id del dispositivo che li ha ricevuti
// invia il file relativo ai messaggi pendenti 
void show(sd) {
    int rId, sId;
    char sUsername[1024], filename[1024];
    recvNum(sd, &rId);
    recvMsg(sd, sUsername);
    // ottengo l'id del dispositivo controllando l'username
    sId = findDevice(sUsername);
    sprintf(filename, "./pending_messages/device_%d/from_%d.txt", rId, sId);
    printf("[SHOW] Sending %s...\n", filename);
    FILE* fp = fopen(filename, "r");
    if (!fp) {
        printf("[SHOW] Nothing to show\n");
        sendNum(sd, ERROR_CODE);
        return;
    }
    sendNum(sd, OK_CODE);
    sendFile(sd, fp);
    printf("[SHOW] Sending %s completed\n", filename);
    fclose(fp);
    remove(filename);
    pend[sId][rId].lastMsgTimestamp = 0;
    pend[sId][rId].num = 0;
    // notifico al dispositivo che aveva inviato i messaggi che i messaggi sono stait letti
    notifyShow(sId);
}

// funzione chiamata in risposta al comando ricevuto dal dispositivo OUT
int out(sd) {
    int id;
    // riceve l'id del dispositivo andato offline
    recvNum(sd, &id);
    if (id == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(sd);
        //handleDevCrash(sd);
        return ERROR_CODE;
    }
    devices[id].online = false;

    printf("[OUT] Dev %d is now offline\n", id);
    // registro l'orario attuale come istante in cui il dispositivo è andato offline
    registerLog(false, id, (unsigned)time(NULL));
    return id;
}

// funzione chiamata in risposta al comando ricevuto dal dispositivo \u
// invia al dispositivo la lista dei dispositivi online, uno per uno
void usernameOnline(sd) {
    int i;
    char emptyLine[] = "\n"; // utilizzato per indicare che la lista è terminata
    for (i = 0; i < nDev; i++) {
        if (devices[i].online) {
            sendMsg(sd, devices[i].username);
        }
    }
    // il dispotivo sa da protocollo che quando ottiene una riga vuota significa
    // che non ci sono ulteriori dispositivi online oltre a quelli già inviati
    sendMsg(sd, emptyLine);
    printf("[USER ONLINE] All online users were sent\n");
}

// funzione che riceve il numero di comando inviato dal dispositivo e svolge le funzioni relative in risposta
void recvCommand(int sd) {
    int command;
    int id; // usato per la busy e not busy
    recvNum(sd, &command);
    if (command == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        close(sd);
        return;
    }
    switch (command) {
    case COMMAND_SIGNUP:
        printf("Command received : [SIGNUP]\n");
        signup(sd);
        break;
    case COMMAND_IN:
        printf("Command received : [IN]\n");
        login(sd);
        break;
    case COMMAND_HANGING:
        printf("Command received : [HANGING]\n");
        hanging(sd);
        break;
    case COMMAND_CHAT:
        printf("Command received : [CHAT]\n");
        // se la funzione restituisce 1 il dispotivo è offline o busy, gestisco la chat offline
        id = deviceData(sd);
        if (id >= 0) {
            printf("[CHAT] %d is busy or offline... preparing for offline chat\n", id);
            prepareChatOffline(sd, id);
        }

        break;
    case COMMAND_SHOW:
        printf("Command received : [SHOW]\n");
        show(sd);
        break;
    case COMMAND_OUT:
        printf("Command received : [OUT]\n");
        out(sd);
        break;
    case COMMAND_ADD:
        printf("Command received : [ADD]\n");
        deviceData(sd);
        break;
    case COMMAND_DEVICE_DATA: // USERNAME
        printf("Command received : [DEVICE DATA]\n");
        usernameOnline(sd);
        break;
    case USER_OFFLINE:
        // qualcuno voleva iniziare una chat con un dispositivo ma non risponde, considero la destinazione
        // offline e inizio una chat con il chiamante
        printf("Command received : [DEVICE CRASHED]]\n");
        id = out(sd); // la out restituisce l'id del dispositivo disconnesso
        if (id == ERROR_CODE)
            return;
        prepareChatOffline(sd, id);
        break;
    // dispositivo segnala di essere passato a busy
    case COMMAND_BUSY:
        recvNum(sd, &id);
        if (id == ERROR_CODE) {
            printf("<ERROR> Something wrong happened\n");
            close(sd);
            return;
        }
        devices[id].busy = true;
        printf("Command received : %d is busy\n", id);
        break;
    // dispositivo segnala di non essere più busy
    case COMMAND_NOT_BUSY:
        recvNum(sd, &id);
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

// legge i comandi da tastiera ricevuti dal server
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

    char path[1024], filename[1024], msg[1024];
    int sId, rId, i;
    struct device* dev;
    // cerco il dispositivo che mi ha scritto confrontando il socket da cui mi scrive con quelli salvati
    for (i = 0; i < nDev; i++) {
        dev = &devices[i];
        if (dev->chatSD == sd) {
            sId = i;
            break;
        }
    }
    rId = dev->rId;
    // dipositivo in chat è crashato
    if (recvMsg(sd, msg) == ERROR_CODE) {
        printf("<ERROR> Something wrong happened\n");
        FD_CLR(dev->chatSD, &master);
        close(dev->chatSD);
        close(sd);
        return;
    }
    // il dispositivo ha effettuato la disconnessione dalla chat, tolgo il socket da quelli ascoltati
    if (!strncmp(msg, "\\q", 2)) {
        FD_CLR(dev->chatSD, &master);
        close(dev->chatSD);
        devices[sId].busy = 0;
        return;
    }
    sprintf(path, "./pending_messages");
    if (stat(path, &st) == -1)
        mkdir(path, 0700);

    //subdirectory per il ricevitore
    sprintf(path, "./pending_messages/device_%d", rId);
    if (stat(path, &st) == -1)
        mkdir(path, 0700);

    // salvo il messaggio ricevuto tra quelli pendenti
    sprintf(filename, "%s/from_%d.txt", path, sId);
    printf("[CHAT] Message saved in :\n\t%s\n", filename);

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
    fclose(fp);

}

/*----------------------------------------------------------------------*\
|                         ***     MAIN     ***                           |
\*----------------------------------------------------------------------*/


int main(int argc, char* argv[]) {
    struct sockaddr_in cl_addr;
    int ret, sd, new_sd, i;
    socklen_t addrlen;
    system("clear");
    printf("********************* SERVER AVVIATO ********************\n");

    switch (argc) {
    // se l'utente non ha inserito la porta le assegno di default la 4242
    case 1: 
        thisPort = 4242;
        break;
    case 2:
        thisPort = atoi(argv[1]);
        break;
    default:
        printf("Syntax error\n");
        exit(-1);
    }
    // ristabilisco le informazioni presenti alla precedente disconnessione del server se presenti
    restoreServer();
    // creo il socket di ascolto a cui potranno scrivere i dispositivi
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
        // controllo quale socket ha ricevuto un messaggio
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
                    addrlen = sizeof(cl_addr);
                    // Accetto nuove connessioni
                    new_sd = accept(sd, (struct sockaddr*)&cl_addr, &addrlen);

                    // Attendo risposta
                    recvCommand(new_sd);
                    continue;
                }
                // un utente sta mandando un messaggio a un dev offline
                handleChat(i);
            }
        }
    }

}