//
//  receiver.c
//  Data Transfer Protocol
//
//  Created by Ashutosh Timilsina on 4/11/20.
//

#include "receiver.h"

#define ECHOMAX 8     /* Longest string to echo */

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
    int dropNum[10];
    int c = 0;

    if ((argc > 12)||(argc < 2))         /* Test for correct number of parameters */
    {
        fprintf(stderr,"Usage:  %s <SERVER PORT> <FORCE DROP NUMBERS(upto 10)>\n", argv[0]);
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
    
    memset(&echoClntAddr, 0, sizeof(echoClntAddr));   /* Zero out structure */

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
    char file[150000] = {0};
    int filesize;
    int cumAckWindow[1000];
    
    int z = 0;
    for (z=0; z < sizeof(cumAckWindow); z++){
        cumAckWindow[z] = 0;
    }
    
    //memset(file, '0', 80);   /* Zero out structure */
    
    int rws = 4;    //receive window size
    int lfr = -1;   //last frame received
    //int * p = dropNum;
    
    while(1){
        /* Receive message from client */
        memset(&rcvMsg, 0, sizeof(rcvMsg));
        memset(&seqNum, 0, sizeof(seqNum));
        memset(&payload, 0, sizeof(payload));
        memset(&flag, 0, sizeof(flag));
        memset(&payloadSize, 0, sizeof(payloadSize));
        
        cliAddrLen = sizeof(echoClntAddr);
        numBytes = recvfrom(sock, &rcvMsg, sizeof(rcvMsg), 0,(struct sockaddr *) &echoClntAddr, &cliAddrLen);
        //printf("Handling client %s\n", inet_ntoa(echoClntAddr.sin_addr));
        //printf("..........RECEIVED %d bytes Packet: %d\n", numBytes, atoi(seqNum));
        
        //Strip Header
        memcpy(seqNum, &rcvMsg[0], 3);
        seqNum[3] = '\0';
        memcpy(payloadSize, &rcvMsg[3], 4);
        payloadSize[4] = '\0';
        memcpy(flag, &rcvMsg[7], 2);
        flag[1] = '\0';
        
        //Strip Payload
        memcpy(payload, &rcvMsg[9], ECHOMAX);
        int frameNum = atoi(seqNum);
        int payloadSizeNum = atoi(payloadSize);
        int lastPkt = atoi(flag);
        int ack;
        int ack_cum;
        
        //Check if the packet is to be dropped
        if (atoi(seqNum) == dropNum[c] && c < 10 && argc > 2){
            printf("RECEIVED Packet #: %d\n.......DROPPED Packet #: %d\n", atoi(seqNum), atoi(seqNum));
            now = time(0);
            fprintf(logfile, "<Dropped> <Seq #: %d> <LFR: %d> <LAF:%d> <Time: %ld>\n", frameNum, lfr, ack_cum, now);
            c += 1;
            int i = 0;
            for (i=0; i < sizeof(cumAckWindow)/sizeof(int); i++){
                if (cumAckWindow[i] == 0){
                    ack_cum = i;
                    break;
                }
            }
            ack = lfr;
        }
        
        else{
            printf("Received Packet #: %d\n", frameNum);
            
            now = time(0);
            fprintf(logfile, "<Received> <Seq #: %d> <LFR: %d> <LAF:%d> <Time: %ld>\n", frameNum, lfr, ack_cum, now);
            
            /* Processing Packet*/
            if (frameNum >= lfr && frameNum <= lfr+1+rws){
                cumAckWindow[frameNum] = 1;
                memcpy(&file[frameNum*ECHOMAX], payload, payloadSizeNum);
                //printf("%d %d\n",frameNum, payloadSizeNum);
                //printf("echo %s\n", file);
                //memcpy(&file[(ack_cum-1)*ECHOMAX], payload, payloadSizeNum);
                ack = frameNum;
                //ack = ack_cum-1;
                
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
            printf(".......SEND ACK: %d sent to %s \n", ack_cum-1, inet_ntoa(echoClntAddr.sin_addr));
            
            now = time(0);
            fprintf(logfile, "<SEND> <ACK #: %d> <LFR: %d> <LAF: %d> <Time: %ld>\n", frameNum, lfr, ack_cum, now);
            
            if (sendto(sock, ackmsg, sizeof(ackmsg), 0,
                                  (struct sockaddr *) &echoClntAddr, sizeof(echoClntAddr)) != sizeof(ackmsg)){
                DieWithError("sendto() sent a different number of bytes than expected");
                
            }
            
            if (lastPkt == 1 || ack_cum == 10){
                printf("Last Packet Received.\n");
                printf("echo: %s\n", file);
                filesize = ((frameNum-1)*ECHOMAX) + payloadSizeNum;
                
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
    }
    /* Task Completed, Close Socket and Exit */
    close(sock);
    return 0;
}
