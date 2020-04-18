//
//  sender.c
//  Data Transfer Protocol
//
//  Created by Ashutosh Timilsina on 4/9/20.
//

#include "sender.h"

#define ECHOMAX         8 //1024     /* Longest string to echo */
#define TIMEOUT_SECS    2       /* Seconds between retransmits */
#define MAXTRIES        5       /* Tries before giving up */

int tries=0;   /* Count of times sent - GLOBAL for signal-handler access */

void DieWithError(char *errorMessage)   /* Error handling function */
{
    perror(errorMessage);
    exit(1);
}
void CatchAlarm(int ignored);            /* Handler for SIGALRM */

/*void listen_ack() {
    char ack[ACK_SIZE];
    int ack_size;
    int ack_seq_num;
    bool ack_error;
    bool ack_neg;

    // Listen for ack from reciever
    while (true) {
        //Setup Select
        struct timeval tv;
        fd_set set;
        
        //set timeout
        tv.tv_sec = 0;
        tv.tv_usec = 500000;
        FD_ZERO(&set)
        FD_SET(sock, &set)
        socklen_t server_addr_size;
        ack_size = recvfrom(socket_fd, (char *)ack, ACK_SIZE,
                MSG_WAITALL, (struct sockaddr *) &server_addr,
                &server_addr_size);
        ack_error = read_ack(&ack_seq_num, &ack_neg, ack);

        window_info_mutex.lock();

        if (!ack_error && ack_seq_num > lar && ack_seq_num <= lfs) {
            if (!ack_neg) {
                window_ack_mask[ack_seq_num - (lar + 1)] = true;
            } else {
                window_sent_time[ack_seq_num - (lar + 1)] = TMIN;
            }
        }

        window_info_mutex.unlock();
    }
}*/

int main(int argc, char *argv[])
{
    int sock;                        /* Socket descriptor */
    struct sockaddr_in echoServAddr; /* Echo server address */
    struct sockaddr_in fromAddr;     /* Source address of echo */
    unsigned short echoServPort;     /* Echo server port */
    unsigned int fromSize;           /* In-out of address size for recvfrom() */
    socklen_t servAddrSize;
    struct sigaction myAction;       /* For setting signal handler */
    char *servIP;                    /* IP address of server */
    char *echoString;                /* String to send to echo server */
    char echoBuffer[ECHOMAX+1];      /* Buffer for echo string */
    int echoStringLen;               /* Length of string to echo */
    int respStringLen;               /* Size of received datagram */
    
    struct data_pkt_t{
        int type;
        int seq_num;
        int length;
        char data[1024];
    };

    if ((argc < 2) || (argc > 4))    /* Test for correct number of arguments */
    {
        fprintf(stderr,"Usage: %s <Server IP> [<Server Port>] <Echo Word>\n", argv[0]);
        exit(1);
    }
    
    FILE *logfile;
    logfile = fopen("senderlog.txt", "w");
    
    //FILE *fp;
    //logfile = fopen("senderlog.txt", "w");

    servIP = argv[1];           /* First arg:  server IP address (dotted quad) */
    if (argc > 2)
        echoServPort = atoi(argv[2]);       /* Use given port, if any */
    else
        echoServPort = 7;  /* 7 is well-known port for echo service */


    if (argc == 4)
        echoString = argv[3];  /* Second arg: string to echo */
    else
        echoString = "We will get through this. We will get through this together."; //Even an invisible thing can create such a devastating damage to human civilization. High Time to think about the development modality we have been following.";  /* some cosntant string */
    
    /*if ((echoStringLen = strlen(echoString)) > ECHOMAX)
        DieWithError("Echo word too long");*/

    /* Create a best-effort datagram socket using UDP */
    if ((sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP)) < 0)
        DieWithError("socket() failed");

    /* Set signal handler for alarm signal */
    myAction.sa_handler = CatchAlarm;
    if (sigfillset(&myAction.sa_mask) < 0) /* block everything in handler */
        DieWithError("sigfillset() failed");
    myAction.sa_flags = 0;

    if (sigaction(SIGALRM, &myAction, 0) < 0)
        DieWithError("sigaction() failed for SIGALRM");

    /* Construct the server address structure */
    memset(&echoServAddr, 0, sizeof(echoServAddr));    /* Zero out structure */
    echoServAddr.sin_family = AF_INET;
    echoServAddr.sin_addr.s_addr = inet_addr(servIP);  /* Server IP address,127.0.0.1 */
    echoServAddr.sin_port = htons(echoServPort);       /* Server port */
    
    /* Construct the client address structure*/
    memset(&fromAddr, 0, sizeof(fromAddr));
    fromAddr.sin_family = AF_INET;
    fromAddr.sin_addr.s_addr = htonl(INADDR_ANY);  /* Client IP address, localhost */
    fromAddr.sin_port = htons(0);       /* Server port*/
    
    /* Bind to the local address*/
      if (bind(sock, (struct sockaddr *) &fromAddr, sizeof(fromAddr)) < 0)
          DieWithError("bind() failed");
    
    
    /* Sliding Window Init */
    time_t now;
    int numbytes = -1;  //number of bytes sent
    int sws = 4;        //window size
    int lar = -1;       //Last Ack Received
    int numPacket = 10;  //(sizeof(echoString) / 8) + 1;
    int finalPktsize = ECHOMAX; //sizeof(echoString) % 8;
    int i = 0;
    int j = 0;
    
    char seq[4]; //seq number
    char payloadSize[5]; //size of data in payload
    char flag[2]; //flag indicating final packet
    char payload[ECHOMAX]; //payload to be sent
    char head[9];
    char closingCount = 0; //counter to keep track of lost final ack
  
    /* Get a response */
    fromSize = sizeof(fromAddr);
    
    while (1){
        for (i = lar+1; i <= (lar + sws) && i <= numPacket; i++){
            /*Prepare Packet*/
            //Clear buffer
            memset(&seq, 0, sizeof(seq));
            memset(&payloadSize, 0, sizeof(payloadSize));
            memset(&flag, 0, sizeof(flag));
            memset(&head, 0, sizeof(head));
            
            //Prepare sequence number
            sprintf(seq, "%3d", i);
            //printf("%d \n", i);
            
            //Payload Size and Flag
            //for final packet
            if (i == numPacket){
                sprintf(payloadSize, "%4d", finalPktsize);
                strcpy(flag, "1");
                closingCount++;
            }
            //for other packets
            else{
                sprintf(payloadSize, "%4d", ECHOMAX);
                strcpy(flag, "0");
            }
            
            strcat(head, seq);
            strcat(head, payloadSize);
            strcat(head, flag);
            
            //Prepare Payload
            int beginIndx = i*ECHOMAX;
            memset(&payload, 0, ECHOMAX);
            memcpy(payload, echoString + beginIndx, ECHOMAX);
            int headerSize = sizeof(head);
            int pktSize = ECHOMAX + headerSize;
            //printf("%s\n",payload);
            
            //Prepare Packet
            char packet[pktSize];
            memset(&packet, 0, pktSize);
            memcpy(&packet[0], head, headerSize);
            memcpy(&packet[9], payload, ECHOMAX);
            
            now = time(0);
            fprintf(logfile, "<SEND> <Packet #: %d> <LAR: %d> <LFS: %d> <Time: %ld>\n", atoi(seq), lar, atoi(seq)-1, now);
            numbytes = sendto(sock, packet, pktSize, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr));
            printf("SEND Packet #: %d\n", atoi(seq));
        }
        if (closingCount >= 10){
            printf("Final ACK Lost.....Closing!\n");
            break;
        }
        
        char ack[7];
        char ack_cum[4];
        char ack_seq_num[4];
        
        //alarm(TIMEOUT_SECS);        /* Set the timeout */

        /* Listen for ack from reciever */
        while (1) {
            //Setup Select
            struct timeval tv;
            fd_set set;
            
            //set timeout
            tv.tv_sec = 2;
            tv.tv_usec = 0;
            FD_ZERO(&set);
            FD_SET(sock, &set);
            
            memset(&ack, 0, sizeof(ack));
            memset(&ack_cum, 0, sizeof(ack_cum));
            memset(&ack_seq_num, 0, sizeof(ack_seq_num));
            
            if (select(sock+1, &set, NULL, NULL, &tv) < 0){
                perror("Select error");
                exit(0);
            }
            
            if (FD_ISSET(sock, &set)){
                //ACK processing
                if (recvfrom(sock, (char *) ack, sizeof(ack), 0, (struct sockaddr*)&echoServAddr, &servAddrSize) < 0){
                    if (tries < MAXTRIES){
                        tries +=1; //need to be checked at last
                        printf("Timed Out. %d more tries ...\n", MAXTRIES-tries);
                        //sleep(1);
                        //alarm(TIMEOUT_SECS);
                    }
                    else{
                        printf("TIMEOUT. Resend the packet.\n");
                        DieWithError("No Response");
                    }
                }
                else {
                    memcpy(ack_seq_num, &ack[0], 3);
                    ack_seq_num[3] = '\0';
                    memcpy(ack_cum, &ack[4], 4);
                    
                    int ackNum = atoi(ack_seq_num);
                    int cumAckNum = atoi(ack_cum);
                    //printf(".........Received ACK %d and CumACK %d \n", ackNum, cumAckNum-1);
                    printf(".........Received ACK %d \n", cumAckNum-1);
                    now = time(0);
                    fprintf(logfile, "........... <RECEIVED> <ACK #: %d> <LAR: %d> <LFS: %d> <Time: %ld>\n", ackNum, lar, atoi(seq), now);
                    if (i==numPacket && cumAckNum == numPacket){
                        printf("Data Transfer Completed!\n");
                        fclose(logfile);
                        close(sock);
                        exit(0);
                    }
                    
                    if (i == numPacket && cumAckNum < numPacket && j > 0){
                        //Prepare Packet
                        //Clear buffer
                        memset(&seq, 0, sizeof(seq));
                        memset(&payloadSize, 0, sizeof(payloadSize));
                        memset(&flag, 0, sizeof(flag));
                        memset(&head, 0, sizeof(head));
                        
                        //Prepare sequence number
                        sprintf(seq, "%3d", cumAckNum);
                        //printf("%d \n", i);
                        
                        //Payload Size and Flag
                        //for final packet
                        if (cumAckNum == numPacket-1){
                            //sprintf(seq, "%3d", lar+1+1);
                            sprintf(payloadSize, "%4d", finalPktsize);
                            strcpy(flag, "1");
                            closingCount++;
                        }
                        //for other packets
                        else{
                            sprintf(payloadSize, "%4d", ECHOMAX);
                            strcpy(flag, "0");
                        }
                        
                        strcat(head, seq);
                        strcat(head, payloadSize);
                        strcat(head, flag);
                        
                        //Prepare Payload
                        int beginIndx = cumAckNum*ECHOMAX;
                        memset(&payload, 0, ECHOMAX);
                        memcpy(payload, echoString + beginIndx, ECHOMAX);
                        int headerSize = sizeof(head);
                        int pktSize = ECHOMAX + headerSize;
                        //printf("%s\n",payload);
                        
                        //Prepare Packet
                        char packet[pktSize];
                        memset(&packet, 0, pktSize);
                        memcpy(&packet[0], head, headerSize);
                        memcpy(&packet[9], payload, ECHOMAX);
                        
                        now = time(0);
                        fprintf(logfile, "<RESEND> <Packet #: %d> <LAR: %d> <LFS: %d> <Time: %ld>\n", atoi(seq), lar, atoi(seq)-1, now);
                        numbytes = sendto(sock, packet, pktSize, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr));
                        printf("RESEND Packet #: %d\n", atoi(seq));
                        //i++;
                    }
                    
                    //Resend the packet if ack = base-1 i.e. duplicate ack
                    if (cumAckNum-1 == lar && cumAckNum < numPacket){
                        j++;
                        //Prepare Packet
                        //Clear buffer
                        memset(&seq, 0, sizeof(seq));
                        memset(&payloadSize, 0, sizeof(payloadSize));
                        memset(&flag, 0, sizeof(flag));
                        memset(&head, 0, sizeof(head));
                        
                        //Prepare sequence number
                        sprintf(seq, "%3d", cumAckNum);
                        //printf("%d \n", i);
                        
                        //Payload Size and Flag
                        //for final packet
                        if (cumAckNum == numPacket-1){
                            //sprintf(seq, "%3d", lar+1+1);
                            sprintf(payloadSize, "%4d", finalPktsize);
                            strcpy(flag, "1");
                            closingCount++;
                        }
                        //for other packets
                        else{
                            sprintf(payloadSize, "%4d", ECHOMAX);
                            strcpy(flag, "0");
                        }
                        
                        strcat(head, seq);
                        strcat(head, payloadSize);
                        strcat(head, flag);
                        
                        //Prepare Payload
                        int beginIndx = cumAckNum*ECHOMAX;
                        memset(&payload, 0, ECHOMAX);
                        memcpy(payload, echoString + beginIndx, ECHOMAX);
                        int headerSize = sizeof(head);
                        int pktSize = ECHOMAX + headerSize;
                        //printf("%s\n",payload);
                        
                        //Prepare Packet
                        char packet[pktSize];
                        memset(&packet, 0, pktSize);
                        memcpy(&packet[0], head, headerSize);
                        memcpy(&packet[9], payload, ECHOMAX);
                        
                        now = time(0);
                        fprintf(logfile, "<RESEND> <Packet #: %d> <LAR: %d> <LFS: %d> <Time: %ld>\n", atoi(seq), lar, atoi(seq)-1, now);
                        numbytes = sendto(sock, packet, pktSize, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr));
                        printf("RESEND Packet #: %d\n", atoi(seq));
                    }
                    
                    //Send the next packet if ack >= base
                    if (cumAckNum-1 > lar && cumAckNum < numPacket && i < numPacket){
                        lar++;
                        //Prepare Packet
                        //Clear buffer
                        memset(&seq, 0, sizeof(seq));
                        memset(&payloadSize, 0, sizeof(payloadSize));
                        memset(&flag, 0, sizeof(flag));
                        memset(&head, 0, sizeof(head));
                        
                        //Prepare sequence number
                        sprintf(seq, "%3d", i);
                        //printf("%d \n", i);
                        
                        //Payload Size and Flag
                        //for final packet
                        if (cumAckNum == numPacket-1 && i == numPacket-1){
                            //sprintf(seq, "%3d", i);
                            sprintf(payloadSize, "%4d", finalPktsize);
                            strcpy(flag, "1");
                            closingCount++;
                        }
                        //for other packets
                        else{
                            sprintf(payloadSize, "%4d", ECHOMAX);
                            strcpy(flag, "0");
                        }
                        
                        strcat(head, seq);
                        strcat(head, payloadSize);
                        strcat(head, flag);
                        
                        //Prepare Payload
                        int beginIndx = i*ECHOMAX;
                        memset(&payload, 0, ECHOMAX);
                        memcpy(payload, echoString + beginIndx, ECHOMAX);
                        int headerSize = sizeof(head);
                        int pktSize = ECHOMAX + headerSize;
                        //printf("%s\n",payload);
                        
                        //Prepare Packet
                        char packet[pktSize];
                        memset(&packet, 0, pktSize);
                        memcpy(&packet[0], head, headerSize);
                        memcpy(&packet[9], payload, ECHOMAX);
                        
                        now = time(0);
                        fprintf(logfile, "<SEND> <Packet #: %d> <LAR: %d> <LFS: %d> <Time: %ld>\n", atoi(seq), lar, atoi(seq)-1, now);
                        numbytes = sendto(sock, packet, pktSize, 0, (struct sockaddr *) &echoServAddr, sizeof(echoServAddr));
                        printf("SEND Packet #: %d\n", atoi(seq));
                        i++;
                    }
                    //lar = cumAckNum - 1;
                    
                }
            }
            else{
                if (tries < MAXTRIES){
                    tries +=1; //need to be checked at last
                    printf("TIMEOUT. Resend window!\n");
                    printf("%d more tries ...\n", MAXTRIES-tries);
                    //sleep(1);
                    //alarm(TIMEOUT_SECS);
                }
                else{
                    printf("TIMEOUT. Closing the program...\n");
                    DieWithError("No Response");
                }
                break;
            }
        }
    }
}

void CatchAlarm(int ignored)     /* Handler for SIGALRM */
{
    tries += 1;
}
