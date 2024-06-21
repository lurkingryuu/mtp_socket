/**
 * @file initmsocket.c
 * 
 * @authors:
 * 1. Vonteri Varshith Reddy (21CS10081)
 * 2. Yelisetty Karthikeya S M (21CS30060)
 * 
 * @date 24-03-2021
 * 
 * @brief This file contains the code for the initialization of the msocket library.
 * It creates a shared memory for storing the socket information and a shared memory for storing the mtp sockets.
 * It also creates semaphores for mutual exclusion and for inter-process communication.
 * It creates three threads: S, R, and G.
 * S is the sender thread which sends messages to the receiver.
 * R is the receiver thread which receives messages from the sender.
 * G is the garbage collector thread which checks whether the process corresponding to any of the MTP sockets is still alive or not.
 * 
 * The sender thread sends messages to the receiver using the corresponding UDP socket.
 * It sets a timer for the message and waits for an ACK message from the receiver.
 * 
 * The receiver thread receives messages from the sender.
 * It checks whether the message is a data message or an ACK message.
 * If it is a data message, it stores the message in the receive buffer.
 * If it is an ACK message, it updates the sender window and sends an ACK message to the sender.
 * 
 * The garbage collector thread checks whether the process corresponding to any of the MTP sockets is still alive or not.
 * If the process is not alive, it cleans up the MTP socket.
 * 
 * The main function creates the threads and does other work like MTP socket creation and binding.
 * 
*/
#include <msocket.h>
#include <pthread.h>
#include <signal.h>

#define MAX(socket1, socket2) ((socket1) > (socket2) ? (socket1) : (socket2))

int sock_info_id;
SOCK_INFO *sock_info;
int sock_info_mutex;
int init_comm_mutex;
struct sembuf pop = {0, -1, SEM_UNDO};
struct sembuf vop = {0, 1, SEM_UNDO};

int sm_id;
mtp_socket *SM;
int sm_mutex;
int sm_id;

pthread_t S_thread, R_thread, G_thread;

const int debug = 1;

// ------------------------------------------ Utility Functions ------------------------------------------
void shm_init()
{
    sock_info_id = shmget(ftok("initmsocket.c", SOCK_INFO_KEY), sizeof(SOCK_INFO), 0666 | IPC_CREAT);
    sock_info = (SOCK_INFO *)shmat(sock_info_id, (void *)0, 0);
    memset(sock_info, 0, sizeof(SOCK_INFO));

    sock_info_mutex = semget(ftok("initmsocket.c", SOCK_INFO_MUTEX_KEY), 1, 0666 | IPC_CREAT);
    semctl(sock_info_mutex, 0, SETVAL, 1);

    sm_id = shmget(ftok("initmsocket.c", MTP_SOCKET_KEY), sizeof(mtp_socket) * MAX_SOCKETS, 0666 | IPC_CREAT);
    SM = (mtp_socket *)shmat(sm_id, (void *)0, 0);
    memset(SM, 0, sizeof(mtp_socket) * MAX_SOCKETS);
    for (int i = 0; i < MAX_SOCKETS; i++)
    {
        SM[i].is_free = 1;
        SM[i].swnd.size = 0;
        SM[i].rwnd.size = 0;
    }
    sm_mutex = semget(ftok("initmsocket.c", MTP_SOCKET_MUTEX_KEY), 1, 0666 | IPC_CREAT);
    semctl(sm_mutex, 0, SETVAL, 1);

    init_comm_mutex = semget(ftok("initmsocket.c", INIT_COMM_MUTEX_KEY), 2, 0666 | IPC_CREAT);
    semctl(init_comm_mutex, 0, SETVAL, 0);
    semctl(init_comm_mutex, 1, SETVAL, 0);

    return;
}

/*
header:
    0-3: sequence number
    4-6: window length
    7: is_ack
*/
char get_header(int seq_num, int win_len, int is_ack)
{
    int bits[8];
    bits[7] = is_ack;
    for (int i = 4; i < 7; i++)
    {
        bits[i] = win_len % 2;
        win_len /= 2;
    }
    for (int i = 0; i < 4; i++)
    {
        bits[i] = seq_num % 2;
        seq_num /= 2;
    }
    char header = 0;
    for (int i = 0; i < 8; i++)
    {
        header = header | (bits[i] << i);
    }
    return header;
}

/*
header:
    0-3: sequence number
    4-6: window length
    7: is_ack
*/
void process_header(char header, int *seq_num, int *win_len, int *is_ack)
{
    *is_ack = (1 << 7 & header) >> 7;
    *win_len = 0;
    for (int i = 4; i < 7; i++)
    {
        *win_len = *win_len | (1 << i & header);
    }
    *win_len = *win_len >> 4;
    *seq_num = 0;
    for (int i = 0; i < 4; i++)
    {
        *seq_num = *seq_num | (1 << i & header);
    }
}

// ------------------------------------------ Threads ------------------------------------------

// Sender Thread
void *S(void *arg)
{
    while (1)
    {
        if (debug)
            ppyellow("[sender] Going to sleep\n");
        sleep(T);
        if (debug)
            ppyellow("[sender] Woke up\n");
        pop.sem_num = 0;
        semop(sm_mutex, &pop, 1); // lock for mutual exclusion
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (SM[i].is_free == 0)
            {
                // if there is a message, send it to the receiver using the corresponding UDP socket
                // set a timer for the message and wait for an ACK message from the receiver

                for (int it = 0; it < SM[i].swnd.size; it++)
                {
                    SM[i].swnd.sequence_numbers[it] = -1;
                }

                {
                    int ptr = 0;
                    for (int j = 0; j < MAX_SEND_BUFFER_SIZE; j++)
                    {
                        if (SM[i].send_buffer[j][0] != '\0')
                        {
                            SM[i].swnd.sequence_numbers[ptr] = SM[i].send_seq_num[j];
                            ptr++;
                        }
                    }
                }

                for (int it = 0; it < SM[i].swnd.size; it++)
                {
                    if (SM[i].swnd.sequence_numbers[it] != -1)
                    {
                        printf(YELLOW "[sender] message in socket:%2d\tseq:%2d\n" RESET, i, SM[i].swnd.sequence_numbers[it]);
                        int index = -1;
                        for (int j = 0; j < MAX_SEND_BUFFER_SIZE; j++)
                        {
                            if (SM[i].send_seq_num[j] == SM[i].swnd.sequence_numbers[it])
                            {
                                index = j;
                                break;
                            }
                        }
                        if (index == -1)
                        {
                            continue;
                        }
                        char buffer[MESSAGE_SIZE + 1];
                        buffer[0] = get_header(SM[i].swnd.sequence_numbers[it], 0, 0);
                        strncpy(buffer + 1, SM[i].send_buffer[index], MESSAGE_SIZE);
                        struct sockaddr_in addr;
                        int len = sizeof(addr);
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(SM[i].dest_port);
                        inet_aton(SM[i].dest_ip, &addr.sin_addr);
                        int n = sendto(SM[i].udp_sock, (const char *)&buffer, MESSAGE_SIZE + 1, 0, (const struct sockaddr *)&addr, len);
                        if (n >= 0){
                            printf(YELLOW"[sender] message sent: "RESET);
                            printf(GREEN"%.3s \n"RESET, buffer + 1);
                        }
                        else
                            pperror("[sender] message not sent\n");
                    }
                }
            }
        }

        vop.sem_num = 0;
        semop(sm_mutex, &vop, 1); // unlock for mutual exclusion
    }
}

// Receiver Thread
void *R(void *arg)
{
    fd_set readfds;
    struct timeval timeout;
    memset(&timeout, 0, sizeof(struct timeval));
    timeout.tv_sec = T;
    timeout.tv_usec = 0;
    while (1)
    {
        // clear the socket set
        FD_ZERO(&readfds);

        // add all the valid mtp sockets to the set
        int max_fd = 0;
        pop.sem_num = 0;
        semop(sm_mutex, &pop, 1); // lock for mutual exclusion
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            if (SM[i].is_free == 0)
            {
                FD_SET(SM[i].udp_sock, &readfds);
                max_fd = MAX(max_fd, SM[i].udp_sock);
            }
        }
        vop.sem_num = 0;
        semop(sm_mutex, &vop, 1); // unlock for mutual exclusion

        int activity = select(max_fd + 1, &readfds, NULL, NULL, &timeout);
        ppmagenta("[receiver] Woke up\n");
        if (activity < 0)
        {
            pperror("[receiver] select() failed");
            pthread_exit(NULL);
        }

        if (activity == 0)
        {
            timeout.tv_sec = T;
            timeout.tv_usec = 0;

            /*
                DUPLICATE ACK MESSAGE WITH THE LAST ACKNOWLEDGED SEQUENCE NUMBER BUT WITH THE UPDATED RWND SIZE
            */
            pop.sem_num = 0;
            semop(sm_mutex, &pop, 1); // lock for mutual exclusion

            // for each socket update receiver window and size of the window and send the ack message
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                int mini = 100000;
                if (SM[i].is_free == 1)
                    continue;

                {
                    int ptr = 0;
                    for (int j = 0; j < MAX_RECEIVE_BUFFER_SIZE; j++)
                    {
                        if (SM[i].receive_buffer[j][0] == '\0')
                        {
                            SM[i].rwnd.sequence_numbers[ptr] = SM[i].receive_seq_num[j];
                            if (mini > SM[i].rwnd.sequence_numbers[ptr])
                            {
                                mini = SM[i].rwnd.sequence_numbers[ptr];
                            }
                            ptr++;
                        }
                    }
                    SM[i].rwnd.size = ptr;
                }
                if (mini == 100000)
                {
                    mini = 1;
                }
                mini--;
                char header = get_header(mini, SM[i].rwnd.size, 1);
                struct sockaddr_in addr;
                int len = sizeof(addr);
                addr.sin_family = AF_INET;
                addr.sin_port = htons(SM[i].dest_port);
                inet_aton(SM[i].dest_ip, &addr.sin_addr);
                int n = sendto(SM[i].udp_sock, (const char *)&header, 1, 0, (const struct sockaddr *)&addr, len);
            }
            // unlock for mutual exclusion
            vop.sem_num = 0;
            semop(sm_mutex, &vop, 1);
        }

        // if there is a message on any of the sockets
        if (activity > 0)
        {
            pop.sem_num = 0;
            semop(sm_mutex, &pop, 1); // lock for mutual exclusion
            for (int i = 0; i < MAX_SOCKETS; i++)
            {
                if (SM[i].is_free == 1)
                    continue;

                if (FD_ISSET(SM[i].udp_sock, &readfds))
                {
                    printf(MAGENTA "[receiver] Message received on socket:%2d\n" RESET, i);
                    char buffer[MESSAGE_SIZE + MESSAGE_HEADER_SIZE + 1];
                    struct sockaddr_in addr;
                    int len = sizeof(addr);
                    int n = recvfrom(SM[i].udp_sock, (char *)buffer, MESSAGE_SIZE + 1, MSG_WAITALL, (struct sockaddr *)&addr, &len);
                    {
                        if (dropMessage(P))
                        {
                            // drop the message
                            ppmagenta("[receiver] 😈 Dropped message 😈\n");
                            continue;
                        }
                    }

                    if (n == -1)
                    {
                        pperror("[receiver] recvfrom() failed in R");
                        continue;
                    }
                    // if it is a data message
                    int seq_num, win_len, is_ack;

                    process_header(buffer[0], &seq_num, &win_len, &is_ack);

                    printf(MAGENTA "[receiver] Received seq_num: %d, win_len: %d, is_ack: %d\n" RESET, seq_num, win_len, is_ack);

                    if (is_ack == 1)
                    {
                        // compare this seq_num with those present in the swnd
                        // if matched then remove it from send buffer
                        // update the swnd.size by the win_len
                        // then rearrange the swnd.sequence_numbers
                        int is_duplicate = 1;
                        for (int it = 0; it < SM[i].swnd.size; it++)
                        {
                            if (SM[i].swnd.sequence_numbers[it] == -1)
                            {
                                continue;
                            }
                            if (SM[i].swnd.sequence_numbers[it] % 16 == seq_num)
                            {
                                int index = -1;
                                for (int j = 0; j < MAX_SEND_BUFFER_SIZE; j++)
                                {
                                    if (SM[i].send_seq_num[j] == SM[i].swnd.sequence_numbers[it])
                                    {
                                        index = j;
                                        break;
                                    }
                                }
                                for (int j = index; j < MAX_SEND_BUFFER_SIZE; j++)
                                {
                                    SM[i].send_seq_num[j] = SM[i].send_seq_num[j + 1];
                                    strncpy(SM[i].send_buffer[j], SM[i].send_buffer[j + 1], MESSAGE_SIZE);
                                }
                                is_duplicate = 0;
                                SM[i].send_seq_num[MAX_SEND_BUFFER_SIZE - 1] = -1;
                                SM[i].send_buffer[MAX_SEND_BUFFER_SIZE - 1][0] = '\0';
                            }
                        }

                        SM[i].swnd.size = win_len;

                        if (is_duplicate == 1) // duplicate ack
                        {
                            ppmagenta("[receiver] Duplicate ack\n");
                        }

                        {
                            int ptr = 0;
                            for (int j = 0; j < MAX_SEND_BUFFER_SIZE; j++)
                            {
                                if (ptr >= SM[i].swnd.size)
                                {
                                    break;
                                }
                                if (SM[i].send_buffer[j][0] != '\0' && SM[i].send_seq_num[j] != -1)
                                {
                                    SM[i].swnd.sequence_numbers[ptr] = SM[i].send_seq_num[j];
                                    ptr++;
                                }
                            }
                            while (ptr < SM[i].swnd.size)
                            {
                                SM[i].swnd.sequence_numbers[ptr] = -1;
                                ptr++;
                            }
                        }
                    }
                    else
                    {
                        // while calling m_recvfrom see the least possible value sequence number available
                        // if the message is not null then return the message and update it with next message
                        // else return ENOMSG
                        ppmagenta("[receiver] Received message\n");

                        {
                            int ptr = 0;
                            for (int j = 0; j < MAX_RECEIVE_BUFFER_SIZE; j++)
                            {
                                if (SM[i].receive_buffer[j][0] == '\0')
                                {
                                    SM[i].rwnd.sequence_numbers[ptr] = SM[i].receive_seq_num[j];
                                    ptr++;
                                }
                            }
                            SM[i].rwnd.size = ptr;
                        }

                        for (int it = 0; it < SM[i].rwnd.size; it++)
                        {
                            if (SM[i].rwnd.sequence_numbers[it] % 16 == seq_num)
                            {
                                int index = -1;
                                for (int j = 0; j < MAX_RECEIVE_BUFFER_SIZE; j++)
                                {
                                    if (SM[i].receive_seq_num[j] == SM[i].rwnd.sequence_numbers[it])
                                    {
                                        index = j;
                                        break;
                                    }
                                }
                                if (index == -1)
                                {
                                    pperror("[receiver] Index not found\n");
                                    break;
                                }
                                strncpy(SM[i].receive_buffer[index], buffer + 1, MESSAGE_SIZE);
                                SM[i].rwnd.size--;
                                break;
                            }
                        }
                        char header = get_header(seq_num, SM[i].rwnd.size, 1);
                        struct sockaddr_in addr;
                        int len = sizeof(addr);
                        addr.sin_family = AF_INET;
                        addr.sin_port = htons(SM[i].dest_port);
                        inet_aton(SM[i].dest_ip, &addr.sin_addr);
                        int n = sendto(SM[i].udp_sock, (const char *)&header, 1, 0, (const struct sockaddr *)&addr, len);
                    }
                }
            }
            vop.sem_num = 0;
            semop(sm_mutex, &vop, 1); // unlock for mutual exclusion
        }
    }
}

/*
Garbage Collector Thread
    it checks whether the process corresponding to any of the MTP sockets is still alive or not
    if the process is not alive, it cleans up the MTP socket
*/
void *G(void *arg)
{
    while (1)
    {
        sleep(GARBAGE_COLLECTOR_INTERVAL);
        for (int i = 0; i < MAX_SOCKETS; i++)
        {
            pop.sem_num = 0;
            semop(sm_mutex, &pop, 1); // lock for mutual exclusion
            if (SM[i].is_free == 0 && SM[i].pid != 0)
            {
                if (kill(SM[i].pid, 0) == -1)
                {
                    ppcyan("[garbage collector] ");
                    printf(CYAN "process %d has been killed, cleaning up MTP socket %d\n" RESET, SM[i].pid, i);
                    SM[i].is_free = 1;
                    SM[i].pid = 0;
                    SM[i].udp_sock = 0;
                    memset(SM[i].source_ip, 0, 16);
                    SM[i].source_port = 0;
                    memset(SM[i].dest_ip, 0, 16);
                    SM[i].dest_port = 0;
                    memset(SM[i].send_buffer, 0, MAX_SEND_BUFFER_SIZE * MESSAGE_SIZE);
                    memset(SM[i].receive_buffer, 0, MAX_RECEIVE_BUFFER_SIZE * MESSAGE_SIZE);
                    SM[i].swnd.size = 0;
                    memset(SM[i].swnd.sequence_numbers, 0, 5 * SEQ_NUM_SIZE);
                    SM[i].rwnd.size = 0;
                    memset(SM[i].rwnd.sequence_numbers, 0, 5 * SEQ_NUM_SIZE);
                }
            }
            vop.sem_num = 0;
            semop(sm_mutex, &vop, 1); // unlock for mutual exclusion
        }
    }
}

// ------------------------------------------ Main Function ------------------------------------------
// signal handler for graceful exit
void exit_handler(int sig)
{
    // kill threads
    pthread_kill(S_thread, SIGKILL);
    pthread_kill(R_thread, SIGKILL);
    pthread_kill(G_thread, SIGKILL);

    // detach and remove shared memory
    shmdt(sock_info);
    shmdt(SM);
    shmctl(sock_info_id, IPC_RMID, NULL);
    shmctl(sm_id, IPC_RMID, NULL);

    // remove semaphores
    semctl(sock_info_mutex, 0, IPC_RMID);
    semctl(sm_mutex, 0, IPC_RMID);
    semctl(init_comm_mutex, 0, IPC_RMID);

    if (sig)
        printf("Exiting gracefully\n");
    exit(0);
}

// main function
int main()
{
    signal(SIGINT, exit_handler);
    shm_init();

    // threads
    // create thread for S
    if (pthread_create(&S_thread, NULL, S, NULL) != 0)
    {
        pperror("pthread_create S failed");
        exit(EXIT_FAILURE);
    }
    // create thread for R
    if (pthread_create(&R_thread, NULL, R, NULL) != 0)
    {
        pperror("pthread_create R failed");
        exit(EXIT_FAILURE);
    }
    // create thread for G (Garbage collector)
    if (pthread_create(&G_thread, NULL, G, NULL) != 0)
    {
        pperror("pthread_create G failed");
        exit(EXIT_FAILURE);
    }

    // Do other work -> MTP socket creation, binding
    while (1)
    {
        pop.sem_num = 0;
        semop(init_comm_mutex, &pop, 1); // wait on Sem1
        pop.sem_num = 0;
        semop(sock_info_mutex, &pop, 1); // lock for mutual exclusion

        if (sock_info->sock_id == 0 && strlen(sock_info->IP) == 0 && sock_info->port == 0)
        {
            ppblue("[main] Sock requested\n");
            int sockfd = socket(AF_INET, SOCK_DGRAM, 0);
            if (sockfd == -1)
            {
                pperror("[main] socket failed");
                sock_info->sock_id = -1;
                sock_info->err_no = errno;
            }
            else
            {
                sock_info->sock_id = sockfd;
            }
        }
        else
        {
            ppblue("[main] Bind requested\n");
            struct sockaddr_in servaddr;
            servaddr.sin_family = AF_INET;
            servaddr.sin_addr.s_addr = inet_addr(sock_info->IP);
            servaddr.sin_port = htons(sock_info->port);
            socklen_t len = sizeof(servaddr);
            int b = bind(sock_info->sock_id, (const struct sockaddr *)&servaddr, len);
            if (b == -1)
            {
                pperror("[main] bind failed");
                sock_info->sock_id = -1;
                sock_info->err_no = errno;
            }
        }

        vop.sem_num = 0;
        semop(sock_info_mutex, &vop, 1); // unlock for mutual exclusion
        vop.sem_num = 1;
        semop(init_comm_mutex, &vop, 1); // signal on Sem2
    }

    exit_handler(0);
    return 0;
}