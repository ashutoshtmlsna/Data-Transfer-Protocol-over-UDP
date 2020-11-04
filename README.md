# Reliable-Data-Transfer-Protocol-using-Sliding-Window

Author: Ashutosh Timilsina  

## Description  
This program implements the sliding window protocol by transferring a string 80 characters or less from a client to a server despite possible packet loss. There are two programs one for client and another for server in this project. The programs are written in C. `sender.c` serves for the client and `receiver.c` serves for the receiver/server side. It is achieved through sliding windows at both client and the server side and packet timeouts is also considered when the client does not receive an ack from the server. The server will buffer packets that are out of order.

## Compilation  
To compile the program, makefile is also provided which can be run using command `$ make` in the working directory with
`sender.c` and `receiver.c`.

## Usage
After compliation two executables are available in the directory which can be run with the usage:  
`$ ./myclient <server_ip> <server_port> <(optional)---string to transfer>`  
`$ ./myserver <server_port> <List of numbers (limited to 10) passed as command line argument to simulate the packet loss>`

## Implementation Details
The client reads server ip, server port and string to be transferred (if available) and then reads the string to be transferred into a buffer. Then buffer is again read into an 8 Byte payload. A header is then attached that contains a 3 byte sequence number, a 4 byte payload size, and a 1 byte final packet flag separated by '\0' so in total 17 byte packet is transferred at each transmission. The type of payload can also be easily incorporated into the header, if required with similar procedure. All in all 10 packets will be transferred after which both client and server will terminate. Packets are sent to server using sliding window protocol, with sliding window of 4, where the server reads these packets into another large buffer and then writes to it to file once the last packet is received as well as echoes it in the terminal.  
The server sends an ack back of 6 bytes, a 3 byte seq number, and a 3 byte Cummulative ACK Number. Timeout is considered for 2 seconds and so if the ACK is not received from Server side, the client will retransmit the window and wait for ACK. 5 tries will be allowed after which program terminates. In case the final ack is lost on transmission, the client retransmits 10 times before closing.
