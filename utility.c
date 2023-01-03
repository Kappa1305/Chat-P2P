
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

#define ERROR_CODE 65535
#define TIMER_SIZE 8
#define MAX_DEVICES 10

void sendNum(int sd, int num) {
    uint16_t netNum = htons(num); // network number
    send(sd, (void*)&netNum, sizeof(uint16_t), 0);
}

int recvNum(int sd) {
    int num;
    uint16_t netNum; // network number
    if (!recv(sd, (void*)&netNum, sizeof(uint16_t), 0)) {
        perror("Error recv number\n");
        return 0; // !!!codice di errore!!!
    }
    num = ntohs(netNum);
    return num;
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
    int len = recvNum(sd);
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
