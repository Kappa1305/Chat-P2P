#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <stdbool.h>
#include <sys/stat.h>


#define TIMER_SIZE 8
#define MAX_DEVICES 10
#define BUFFER_SIZE 1024
#define WORD_SIZE 1024


#define ERROR_CODE          65535
#define OK_CODE             65534
#define QUIT_CODE           65533
#define USER_CODE           65532
#define ADD_CODE            65531
#define SHARE_CODE          65530
#define HELP_CODE           65529
#define CLEAR_CODE          65528
#define USER_BUSY           65527
#define USER_OFFLINE        65526
#define USER_NOT_FOUND      65525
#define SHARE_ERROR         65524
#define ADD_ERROR           65523
#define COMMAND_SIGNUP      1
#define COMMAND_IN          2
#define COMMAND_HANGING     3
#define COMMAND_CHAT        4
#define COMMAND_DEVICE_DATA 5
#define COMMAND_SHOW        6
#define COMMAND_OUT         7
#define COMMAND_ADD         8
#define COMMAND_BUSY        10
#define COMMAND_NOT_BUSY    11

int createSocket(int port) {
    int sd;
    struct sockaddr_in srv_addr;
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
        printf("<ERROR> Dev might be offline, try again later...\n");
        // dire al server che il dispositivo è offline
        return ERROR_CODE;
    }
    return sd;
}

void sendNum(int sd, int num) {
    uint16_t netNum = htons(num); // network number
    send(sd, (void*)&netNum, sizeof(uint16_t), 0);
}

int recvNum(int sd, int* num) {
    uint16_t netNum; // network number
    if (!recv(sd, (void*)&netNum, sizeof(uint16_t), 0)) {
        perror("Error recv number\n");
        return ERROR_CODE;
    }
    *num = ntohs(netNum);
    return 0;
}


int sendMsg(int sd, char* msg) {
    int len = strlen(msg);
    char buffer[len];
    strcpy(buffer, msg);

    sendNum(sd, len);

    if (!send(sd, (void*)buffer, strlen(buffer), 0)) {
        perror("Error send msg \n");
        return(ERROR_CODE);
    }
    return 0;
}

int recvMsg(int sd, char* msg) {
    int len;
    recvNum(sd, &len);
    char buffer[len];

    if (!recv(sd, (void*)&buffer, len, 0)) {
        perror("Error rcv msg \n");
        return(ERROR_CODE);
    }
    buffer[len] = '\0';
    strcpy(msg, buffer);
    msg = buffer;
    return 0;
}

void recvFile(int sd, char type[WORD_SIZE]) {

    FILE* fp;
    char buffer[BUFFER_SIZE];

    char namefile[WORD_SIZE];
    sprintf(namefile, "recv.%s", type);
    fp = fopen(namefile, "w");

    while (true) {
        int code;
        recvNum(sd, &code);
        if (code == OK_CODE) {
            recv(sd, buffer, BUFFER_SIZE, 0);
            fprintf(fp, "%s", buffer);
            bzero(buffer, BUFFER_SIZE);
        }
        else {
            fclose(fp);
            return;
        }
    }
}

int sendFile(int sd, FILE* fp) {
    char buff[BUFFER_SIZE] = { 0 };
    while (true) {
        if (fgets(buff, 4096, fp) != NULL) {
            sendNum(sd, OK_CODE);
            if (send(sd, buff, sizeof(buff), 0) == -1) {
                perror("[SHARE] Error!\n");
                return ERROR_CODE;
            }
            bzero(buff, BUFFER_SIZE);
        }
        else {
            sendNum(sd, SHARE_ERROR);
            return 0;
        }
    }
}