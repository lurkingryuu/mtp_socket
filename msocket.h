/**
 * @file msocket.h
 * 
 * @authors:
 * 1. Vonteri Varshith Reddy (21CS10081)
 * 2. Yelisetty Karthikeya S M (21CS30060)
 * 
 * @brief This file contains the declarations of the functions and the structures used in the implementation of the MTP socket.
 * The documentation for the functions can be found in documentation.txt
*/
#ifndef _MSOCKET_H
#define _MSOCKET_H

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <pthread.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <fcntl.h>

#include <netinet/in.h>
#include <arpa/inet.h>

#include <sys/time.h>
#include <sys/ipc.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/select.h>
#include <sys/time.h>
#include <sys/wait.h>

#define MAX_SOCKETS 25
#define MAX_SEND_BUFFER_SIZE 10
#define MAX_RECEIVE_BUFFER_SIZE 5
#define MESSAGE_SIZE 1024
#define MESSAGE_HEADER_SIZE 1
#define SEQ_NUM_SIZE 4
#define GARBAGE_COLLECTOR_INTERVAL 5

#define MAX_WINDOW_SIZE 5

// MTP socket type
#define SOCK_MTP 7

// Timeout in seconds
#define T 5

// Probability of dropping a message
#define P 0.0

// Structure for sender window
typedef struct swnd
{
    int size;
    int sequence_numbers[MAX_WINDOW_SIZE];
} swnd;

// Structure for receiver window
typedef struct rwnd
{
    int size;
    int sequence_numbers[MAX_WINDOW_SIZE];
} rwnd;

// Structure for MTP socket
typedef struct mtp_socket
{
    int is_free;
    int pid;
    int udp_sock;
    char source_ip[16];
    int source_port;
    char dest_ip[16];
    int dest_port;
    char send_buffer[MAX_SEND_BUFFER_SIZE][MESSAGE_SIZE];
    int send_seq_num[MAX_SEND_BUFFER_SIZE];
    char receive_buffer[MAX_RECEIVE_BUFFER_SIZE][MESSAGE_SIZE];
    int receive_seq_num[MAX_RECEIVE_BUFFER_SIZE];
    swnd swnd;
    rwnd rwnd;
    int num_messages_sent;
} mtp_socket;

// Structure for shared memory
typedef struct sock_info
{
    int sock_id;
    char IP[16];
    int port;
    int err_no;
} SOCK_INFO;

// Shared Memory keys
#define SOCK_INFO_KEY 65
#define SOCK_INFO_MUTEX_KEY 66
#define MTP_SOCKET_KEY 67
#define MTP_SOCKET_MUTEX_KEY 68
#define INIT_COMM_MUTEX_KEY 69

// Utility functions

// Function to create a new MTP socket
// type must be SOCK_MTP
// Returns the socket id on success, -1 on failure
int m_socket(int domain, int type, int protocol);

// Function to bind the MTP socket to a specific address
// Returns 0 on success, -1 on failure
int m_bind(int sockfd, char *source_ip, int source_port, char *dest_ip, int dest_port);

// Function to send a message to the MTP socket
// Returns the number of bytes sent on success, -1 on failure
int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen);

// Function to receive a message from the MTP socket
// Returns the number of bytes received on success, -1 on failure
int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen);

// Function to close the MTP socket
// Returns 0 on success, -1 on failure
int m_close(int sockfd);

// Function to print the information of the MTP socket
void prinfo();

// Function to drop a message with probability p
int dropMessage(float p);

// MISCELLANEOUS definitions

// Colors for printing
#define RED "\033[0;31m"
#define GREEN "\033[0;32m"
#define YELLOW "\033[0;33m"
#define BLUE "\033[0;34m"
#define MAGENTA "\033[0;35m"
#define CYAN "\033[0;36m"
#define RESET "\033[0m"

// Function to print the error message
#define pperror(msg) perror(RED msg RESET)
#define ppgreen(msg) printf(GREEN msg RESET)
#define ppblue(msg) printf(BLUE msg RESET)
#define ppyellow(msg) printf(YELLOW msg RESET)
#define ppmagenta(msg) printf(MAGENTA msg RESET)
#define ppcyan(msg) printf(CYAN msg RESET)

#endif // _MSOCKET_H