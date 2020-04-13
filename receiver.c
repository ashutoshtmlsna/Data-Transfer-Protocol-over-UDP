//
//  receiver.c
//  Data Transfer Protocol
//
//  Created by Ashutosh Timilsina on 4/11/20.
//

#include "receiver.h"

#define ECHOMAX 1024     /* Longest string to echo */

void DieWithError(char *errorMessage)  /* External error handling function */
{
    perror(errorMessage);
    exit(1);
}
void stripHeader(char packet[], char head[]);

int main(int argc, char *argv[])
{
    int sock;                        /* Socket */
    struct sockaddr_in echoServAddr; /* Local address */
    struct sockaddr_in echoClntAddr; /* Client address */
    unsigned int cliAddrLen;         /* Length of incoming message */
    char echoBuffer[ECHOMAX];        /* Buffer for echo string, replaced by rcvMsg */
    unsigned short echoServPort;     /* Server port */
    int recvMsgSize;                 /* Size of received message */
    int dropNum[3];

    if ((argc > 5)||(argc < 2))         /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <SERVER PORT> <FORCE DROP NUMBERS(upto 3)>\n", argv[0]);
        exit(1);
    }

    echoServPort = atoi(argv[1]);  /* First arg:  local port */
    if (argc > 2){
        for (int i = 2; i < argc; i++){
            dropNum[i-2] = atoi(argv[i]);
        }
    }

    /* Create socket for sending/receiving datagrams */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    
    FILE *logfile;
    logfile = fopen("receiverlog.txt", "w");
    
    /* Construct local address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));   /* Zero out structure */
    echoServAddr.sin_family = AF_INET;                /* Internet address family */
    echoServAddr.sin_addr.s_addr = htonl(INADDR_ANY); /* Any incoming interface */
    echoServAddr.sin_port = htons(echoServPort);      /* Local port */

    /* Bind to the local address */
    if (bind(sock, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr)) < 0)
        DieWithError("bind() failed");
  
    time_t now;
    int numBytes;
    char rcvMsg[1033]; //1024+9
    char seqNum[4];
    char payloadSize[5];
    char flag[2];
    char payload[1024];
    char file[150000];
    int filesize;
    int cumAckWindow[1000];
    
    int z = 0;
    for (z=0; z < sizeof(cumAckWindow); z++){
        cumAckWindow[z] = 0;
    }
    
    int rws = 9;    //receive window size
    int lfr = -1;   //last frame received
    
    while(1){
        /* Receive message from client */
        memset(&rcvMsg, 0, sizeof(rcvMsg));
        memset(&seqNum, 0, sizeof(seqNum));
        memset(&payload, 0, sizeof(payload));
        memset(&flag, 0, sizeof(flag));
        memset(&payloadSize, 0, sizeof(payloadSize));
        
        cliAddrLen = sizeof(echoClntAddr);
        numBytes = recvfrom(sock, &rcvMsg, sizeof(rcvMsg), 0,(struct sockaddr *) &echoClntAddr, &cliAddrLen);
        printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));
        printf("RECEIVED %d bytes\n", numBytes);
        
        //Strip Header
        memcpy(seqNum, &rcvMsg[0], 3);
        seqNum[3] = '\0';
        memcpy(payloadSize, &rcvMsg[3], 4);
        payloadSize[4] = '\0';
        memcpy(flag, &rcvMsg[7], 2);
        flag[1] = '\0';
        
        //Strip Payload
        memcpy(payload, &rcvMsg[9], 1024);
        
        int frameNum = atoi(seqNum);
        int payloadSizeNum = atoi(payloadSize);
        int lastPkt = atoi(flag);
        int ack;
        int ack_cum;
        printf(".......Received Packet #: %d\n", frameNum);
        
        now = time(0);
        fprintf(logfile, "<Received> <Seq #: %d> <LFR: %d> <LAF:%d> <Time: %ld>\n", frameNum, lfr, ack_cum, now);
        
        /* Processing Packet*/
        if (frameNum >= lfr && frameNum <= lfr+1+rws){
            cumAckWindow[frameNum] = 1;
            memcpy(&file[frameNum*1024], payload, payloadSizeNum);
            ack = frameNum;
            
            int i = 0;
            for (i=0; i < sizeof(cumAckWindow)/sizeof(int); i++){
                if (cumAckWindow[i] == 0){
                    ack_cum = i;
                    break;
                }
            }
            lfr = ack_cum -1;
        }
        else{
            int i = 0;
            for (i=0; i < sizeof(cumAckWindow)/sizeof(int); i++){
                if (cumAckWindow[i] == 0){
                    ack_cum = i;
                    break;
                }
            }
            ack = lfr;
        }
        
        /* Prepare ACK */
        char ack_copy[4];
        char ack_cum_copy[4];
        char ackmsg[7];
        sprintf(ack_copy, "%3d", ack);
        sprintf(ack_cum_copy, "%3d", ack_cum);
        strcpy(ackmsg, ack_copy);
        strcat(ackmsg, ack_cum_copy);
        printf("ACK: %s\n", ackmsg);
        
        now = time(0);
        fprintf(logfile, "<SEND> <ACK #: %d> <LFR: %d> <LAF: %d> <Time: %ld>\n", frameNum, lfr, ack_cum, now);
        
        numBytes = sendto(sock, ackmsg, sizeof(ackmsg), 0, (struct sockaddr *) &echoClntAddr, cliAddrLen);
        
        if (lastPkt == 1){
            printf("Last Packet Received.\n");
            filesize = ((frameNum-1)*1024) + payloadSizeNum;
            
            FILE *outFilePtr;
            outFilePtr = fopen("receivedFile.txt","wb");
            
            if (fwrite(file, 1, filesize, outFilePtr) != filesize || outFilePtr == NULL){
                printf("Error writing to file!\n");
            }
            fclose(outFilePtr);
            fclose(logfile);
            break;
        }
    }
    
    /*for (;;) /* Run forever
    {
        /* Set the size of the in-out parameter
        cliAddrLen = sizeof(echoClntAddr);

        /* Block until receive message from a client
        if ((recvMsgSize = recvfrom(sock, echoBuffer, ECHOMAX, 0,
            (struct sockaddr *) &echoClntAddr, &cliAddrLen)) < 0)
            DieWithError("recvfrom() failed");

        printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));

        /* Send received datagram back to the client
        if (sendto(sock, echoBuffer, recvMsgSize, 0,
             (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != recvMsgSize)
            DieWithError("sendto() sent a different number of bytes than expected");
    }*/
    /* NOT REACHED */
    close(sock);
    return 0;
}
