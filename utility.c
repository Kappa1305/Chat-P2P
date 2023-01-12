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
#define COMMAND_SIGNUP      1
#define COMMAND_IN          2
#define COMMAND_HANGING     3
#define COMMAND_CHAT        4

#define COMMAND_DEVICE_DATA 5
#define COMMAND_OUT         7
#define COMMAND_BUSY        10
#define COMMAND_NOT_BUSY    11

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

void recv_file(int sd, char type[WORD_SIZE]){

    FILE *fp;
    int n;
    char buffer[BUFFER_SIZE];

    char namefile[WORD_SIZE];
    sprintf(namefile, "recv.%s", type);

    fp = fopen(namefile, "w");

    while(true){
        int code = recvNum(sd, false);
        if(code == OK_CODE){
            n = recv(sd, buffer, BUFFER_SIZE, 0);
            fprintf(fp, "%s", buffer);
            bzero(buffer, BUFFER_SIZE);
        }
        else{
            fclose(fp);
            return;
        }
    }
}

void sendFile(int sd,FILE* fp) {
    while (true) {
        if (fgets(buff, BUFFER_SIZE, fp) != NULL) {
            sendNum(sd,OK_CODE);
            if (send(sd, buff, sizeof(buff), 0) == -1) {
                perror("[SHARE] Error!\n");
                exit(1);
            }
            bzero(buff, BUFFER_SIZE);
        }
        else {
            sendNum(sd, SHARE_ERROR);
            return;
        }
    }
}