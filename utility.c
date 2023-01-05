
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

#define TIMER_SIZE 8
#define MAX_DEVICES 10


#define ERROR_CODE            65535
#define OK_CODE             65534
#define QUIT_CODE           65533
#define USER_CODE           65532
#define ADD_CODE            65531
#define SHARE_CODE          65530
#define HELP_CODE           65529
#define CLEAR_CODE          65528
#define BUSY_CODE           65527


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
