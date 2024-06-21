/**
 * @file msocket.c
 * 
 * @authors:
 * 1. Vonteri Varshith Reddy (21CS10081)
 * 2. Yelisetty Karthikeya S M (21CS30060)
 * 
 * @brief This file contains the implementation of the functions for the msocket library.
 * The documentation for the functions can be found in documentation.txt
*/
#include <msocket.h>

SOCK_INFO *m_sock_info;
int m_sock_info_mutex;
int m_init_comm_mutex;
struct sembuf m_pop = {0, -1, 0};
struct sembuf m_vop = {0, 1, 0};

mtp_socket *m_SM = NULL;
int m_sm_shmid;
int m_sm_mutex;
int m_debug = 0;

int m_socket(int domain, int type, int protocol)
{
    if (type != SOCK_MTP)
    {
        errno = ENOTSUP;
        return -1;
    }
    // ----------------------------- Check if there is a free entry in m_SM -----------------------------
    m_sm_mutex = semget(ftok("initmsocket.c", MTP_SOCKET_MUTEX_KEY), 1, 0);
    m_pop.sem_num = 0;
    semop(m_sm_mutex, &m_pop, 1); // wait on m_sm_mutex
    m_sm_shmid = shmget(ftok("initmsocket.c", MTP_SOCKET_KEY), sizeof(mtp_socket) * MAX_SOCKETS, 0);
    m_SM = (mtp_socket *)shmat(m_sm_shmid, (void *)0, 0);

    int i;
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (m_SM[i].is_free == 1)
        {
            break;
        }
    }

    if (i >= MAX_SOCKETS)
    {

        // signal m_sm_mutex
        semop(m_sm_mutex, &m_vop, 1);

        // free resources
        shmdt(m_SM);
        errno = ENOBUFS;

        return -1;
    }

    // signal m_sm_mutex
    semop(m_sm_mutex, &m_vop, 1);

    // ----------------------------- Create UDP socket via initmsocket.c -----------------------------
    m_init_comm_mutex = semget(ftok("initmsocket.c", INIT_COMM_MUTEX_KEY), 2, 0);
    m_sock_info_mutex = semget(ftok("initmsocket.c", SOCK_INFO_MUTEX_KEY), 1, 0);
    m_pop.sem_num = 0;
    semop(m_sock_info_mutex, &m_pop, 1); // wait on m_sock_info_mutex
    int sock_info_shmid = shmget(ftok("initmsocket.c", SOCK_INFO_KEY), sizeof(SOCK_INFO), 0);
    m_sock_info = (SOCK_INFO *)shmat(sock_info_shmid, (void *)0, 0);
    memset(m_sock_info, 0, sizeof(SOCK_INFO));
    m_vop.sem_num = 0;
    semop(m_sock_info_mutex, &m_vop, 1); // signal m_sock_info_mutex

    // signal sem1
    m_vop.sem_num = 0;
    semop(m_init_comm_mutex, &m_vop, 1);

    // wait on sem2
    m_pop.sem_num = 1;
    semop(m_init_comm_mutex, &m_pop, 1);

    if (m_sock_info->sock_id == -1)
    {
        int err_no = m_sock_info->err_no;

        // free resources
        shmdt(m_sock_info);
        shmdt(m_SM);
        errno = err_no;
        return -1;
    }

    m_SM[i].is_free = 0;
    m_SM[i].udp_sock = m_sock_info->sock_id;
    m_SM[i].pid = getpid();

    if (m_debug)
        printf("[msocket.c] Socket Created %d=>%d pid:%d\n", i, m_SM[i].udp_sock, m_SM[i].pid);

    // initialize the send and receive windows
    for (int j = 0; j < MAX_RECEIVE_BUFFER_SIZE; j++)
    {
        memset(m_SM[i].receive_buffer[j], 0, MESSAGE_SIZE);
    }
    for (int j = 0; j < MAX_SEND_BUFFER_SIZE; j++)
    {
        memset(m_SM[i].send_buffer[j], 0, MESSAGE_SIZE);
    }
    m_SM[i].swnd.size = 5;
    for (int j = 0; j < 5; j++)
    {
        m_SM[i].swnd.sequence_numbers[j] = -1;
    }
    m_SM[i].num_messages_sent = 0;
    m_SM[i].rwnd.size = 5;
    for (int j = 0; j < 5; j++)
    {
        m_SM[i].receive_seq_num[j] = j + 1;
    }
    // free resources
    shmdt(m_sock_info);
    shmdt(m_SM);

    return i;
}

int m_bind(int sockfd, char *source_ip, int source_port, char *dest_ip, int dest_port)
{
    // ----------------------------- Find the corresponding actual UDP socket id from the m_SM table -----------------------------
    m_sm_mutex = semget(ftok("initmsocket.c", MTP_SOCKET_MUTEX_KEY), 1, 0666 | IPC_CREAT);
    if (sockfd < 0 || sockfd >= MAX_SOCKETS)
    {
        errno = EBADF;
        return -1;
    }
    // wait on m_sm_mutex
    semop(m_sm_mutex, &m_pop, 1);
    m_sm_shmid = shmget(ftok("initmsocket.c", MTP_SOCKET_KEY), sizeof(mtp_socket) * MAX_SOCKETS, 0);
    m_SM = (mtp_socket *)shmat(m_sm_shmid, (void *)0, 0);
    int udp_sock = m_SM[sockfd].udp_sock;

    // if the UDP socket ID is 0, then it is not initialized
    if (udp_sock == 0 || udp_sock == -1)
    {

        // signal m_sm_mutex
        semop(m_sm_mutex, &m_vop, 1);

        // free resources
        shmdt(m_SM);
        errno = ENOTSOCK;

        return -1;
    }
    // signal m_sm_mutex
    semop(m_sm_mutex, &m_vop, 1);

    // ----------------------------- Put the UDP socket ID, IP, and port in SOCK_INFO table -----------------------------
    m_init_comm_mutex = semget(ftok("initmsocket.c", INIT_COMM_MUTEX_KEY), 2, 0666 | IPC_CREAT);
    m_sock_info_mutex = semget(ftok("initmsocket.c", SOCK_INFO_MUTEX_KEY), 1, 0666 | IPC_CREAT);

    m_pop.sem_num = 0;
    semop(m_sock_info_mutex, &m_pop, 1); // wait on m_sock_info_mutex
    int sock_info_shmid = shmget(ftok("initmsocket.c", SOCK_INFO_KEY), sizeof(SOCK_INFO), 0);
    m_sock_info = (SOCK_INFO *)shmat(sock_info_shmid, (void *)0, 0);
    // set SOCK_INFO fields
    m_sock_info->sock_id = udp_sock;
    strcpy(m_sock_info->IP, source_ip);
    m_sock_info->port = source_port;
    m_vop.sem_num = 0;
    semop(m_sock_info_mutex, &m_vop, 1); // signal m_sock_info_mutex

    m_vop.sem_num = 0;
    semop(m_init_comm_mutex, &m_vop, 1); // signal sem1
    m_pop.sem_num = 1;
    semop(m_init_comm_mutex, &m_pop, 1); // wait on sem2

    if (m_sock_info->sock_id == -1)
    {
        int error_no = m_sock_info->err_no;

        // reset all fields of SOCK_INFO to 0
        m_pop.sem_num = 0;
        semop(m_sock_info_mutex, &m_pop, 1); // wait on m_sock_info_mutex
        memset(m_sock_info, 0, sizeof(SOCK_INFO));
        m_vop.sem_num = 0;
        semop(m_sock_info_mutex, &m_vop, 1); // signal m_sock_info_mutex

        // free resources
        shmdt(m_sock_info);
        shmdt(m_SM);

        errno = error_no;
        return -1;
    }

    // valid bind done
    m_SM[sockfd].source_port = source_port;
    strcpy(m_SM[sockfd].source_ip, source_ip);
    m_SM[sockfd].dest_port = dest_port;
    strcpy(m_SM[sockfd].dest_ip, dest_ip);

    // reset all fields of SOCK_INFO to 0
    m_pop.sem_num = 0;
    semop(m_sock_info_mutex, &m_pop, 1); // wait on m_sock_info_mutex
    memset(m_sock_info, 0, sizeof(SOCK_INFO));
    m_vop.sem_num = 0;
    semop(m_sock_info_mutex, &m_vop, 1); // signal m_sock_info_mutex

    // free resources
    shmdt(m_sock_info);
    shmdt(m_SM);

    return 0;
}

int m_sendto(int sockfd, const void *buf, size_t len, int flags, const struct sockaddr *dest_addr, socklen_t addrlen)
{
    if (sockfd < 0 || sockfd >= MAX_SOCKETS)
    {
        errno = EBADF;
        return -1;
    }
    m_sm_mutex = semget(ftok("initmsocket.c", MTP_SOCKET_MUTEX_KEY), 1, 0666 | IPC_CREAT);
    m_pop.sem_num = 0;
    semop(m_sm_mutex, &m_pop, 1); // wait on m_sm_mutex
    m_sm_shmid = shmget(ftok("initmsocket.c", MTP_SOCKET_KEY), sizeof(mtp_socket) * MAX_SOCKETS, 0);
    m_SM = (mtp_socket *)shmat(m_sm_shmid, (void *)0, 0);

    // check if the socket is valid
    if (m_SM[sockfd].is_free == 1)
    {
        // signal m_sm_mutex
        m_vop.sem_num = 0;
        semop(m_sm_mutex, &m_vop, 1);
        shmdt(m_SM);
        errno = EBADF;
        return -1;
    }

    // ----------------------------- Check if the send to address is valid bound address -----------------------------
    if (strcmp(m_SM[sockfd].dest_ip, inet_ntoa(((struct sockaddr_in *)dest_addr)->sin_addr)) != 0 || m_SM[sockfd].dest_port != ntohs(((struct sockaddr_in *)dest_addr)->sin_port))
    {
        // signal m_sm_mutex
        m_vop.sem_num = 0;
        semop(m_sm_mutex, &m_vop, 1);
        // free resources
        shmdt(m_SM);
        errno = ENOTCONN;
        return -1;
    }

    // ----------------------------- Check if there is space in the send buffer -----------------------------
    int i;
    for (i = 0; i < MAX_SEND_BUFFER_SIZE; i++)
    {
        if (m_SM[sockfd].send_buffer[i][0] == '\0')
        {
            break;
        }
    }

    if (i >= MAX_SEND_BUFFER_SIZE)
    {

        // signal m_sm_mutex
        semop(m_sm_mutex, &m_vop, 1);

        // free resources
        shmdt(m_SM);

        errno = ENOBUFS;
        return -1;
    }

    // ----------------------------- Write the message to the sender side message buffer -----------------------------
    memset(m_SM[sockfd].send_buffer[i], 0, MESSAGE_SIZE);
    strncpy(m_SM[sockfd].send_buffer[i], (char *)buf, len);
    m_SM[sockfd].num_messages_sent++;
    m_SM[sockfd].send_seq_num[i] = m_SM[sockfd].num_messages_sent;
    if (m_debug)
        printf("[msocket.c] Message sent: %s\n", m_SM[sockfd].send_buffer[i]);
    // signal m_sm_mutex
    m_vop.sem_num = 0;
    semop(m_sm_mutex, &m_vop, 1);

    // free resources
    shmdt(m_SM);

    return 0;
}

int m_recvfrom(int sockfd, void *buf, size_t len, int flags, struct sockaddr *src_addr, socklen_t *addrlen)
{
    if (sockfd < 0 || sockfd >= MAX_SOCKETS)
    {
        errno = EBADF;
        return -1;
    }
    m_sm_mutex = semget(ftok("initmsocket.c", MTP_SOCKET_MUTEX_KEY), 1, 0666 | IPC_CREAT);
    m_pop.sem_num = 0;
    semop(m_sm_mutex, &m_pop, 1); // wait on m_sm_mutex
    m_sm_shmid = shmget(ftok("initmsocket.c", MTP_SOCKET_KEY), sizeof(mtp_socket) * MAX_SOCKETS, 0);
    m_SM = (mtp_socket *)shmat(m_sm_shmid, (void *)0, 0);

    // check if the socket is valid
    if (m_SM[sockfd].is_free == 1)
    {
        // signal m_sm_mutex
        m_vop.sem_num = 0;
        semop(m_sm_mutex, &m_vop, 1);
        shmdt(m_SM);
        errno = EBADF;
        return -1;
    }

    // find the minimum sequence number in the receive buffer
    int min_seq_num = 1e9;
    int max_seq_num = -1;
    int min_seq_num_index = -1;
    for (int i = 0; i < MAX_RECEIVE_BUFFER_SIZE; i++)
    {
        if (m_SM[sockfd].receive_seq_num[i] < min_seq_num)
        {
            min_seq_num = m_SM[sockfd].receive_seq_num[i];
            min_seq_num_index = i;
        }
        if (m_SM[sockfd].receive_seq_num[i] > max_seq_num)
        {
            max_seq_num = m_SM[sockfd].receive_seq_num[i];
        }
    }
    if (m_SM[sockfd].receive_buffer[min_seq_num_index][0] == '\0')
    {
        // signal m_sm_mutex
        m_vop.sem_num = 0;
        semop(m_sm_mutex, &m_vop, 1);
        errno = ENOMSG;
        return -1;
    }

    // copy the message and send message
    // memset((char *)buf, 0, MESSAGE_SIZE);
    strncpy((char *)buf, m_SM[sockfd].receive_buffer[min_seq_num_index], MESSAGE_SIZE);
    memset(m_SM[sockfd].receive_buffer[min_seq_num_index], 0, MESSAGE_SIZE);
    m_SM[sockfd].receive_seq_num[min_seq_num_index] = max_seq_num + 1;
    if (m_debug)
        printf("[msocket.c] Message received: %s\n", (char *)buf);
    // signal m_sm_mutex
    m_vop.sem_num = 0;
    semop(m_sm_mutex, &m_vop, 1);
    // free resources
    shmdt(m_SM);

    return MESSAGE_SIZE;
}

int m_close(int sockfd)
{
    // ----------------------------- Find the corresponding actual UDP socket id from the m_SM table -----------------------------
    m_sm_mutex = semget(ftok("initmsocket.c", MTP_SOCKET_MUTEX_KEY), 1, 0666 | IPC_CREAT);
    if (sockfd < 0 || sockfd >= MAX_SOCKETS)
    {
        errno = EBADF;
        return -1;
    }
    // wait on m_sm_mutex
    semop(m_sm_mutex, &m_pop, 1);
    m_sm_shmid = shmget(ftok("initmsocket.c", MTP_SOCKET_KEY), sizeof(mtp_socket) * MAX_SOCKETS, 0);
    m_SM = (mtp_socket *)shmat(m_sm_shmid, (void *)0, 0);

    int udp_sock = m_SM[sockfd].udp_sock;
    // if the UDP socket ID is 0, then it is not initialized
    if (udp_sock == 0 || udp_sock == -1)
    {

        // signal m_sm_mutex
        semop(m_sm_mutex, &m_vop, 1);

        // free resources
        shmdt(m_SM);
        errno = ENOTSOCK;

        return -1;
    }
    // signal m_sm_mutex
    semop(m_sm_mutex, &m_vop, 1);
    // mark the entry as free
    m_SM[sockfd].is_free = 1;
    // free resources
    shmdt(m_SM);

    return 0;
}

void prinfo()
{
    pid_t pid = getpid();
    int i;

    m_sm_mutex = semget(ftok("initmsocket.c", MTP_SOCKET_MUTEX_KEY), 1, 0666 | IPC_CREAT);
    m_sm_shmid = shmget(ftok("initmsocket.c", MTP_SOCKET_KEY), sizeof(mtp_socket) * MAX_SOCKETS, 0);
    m_SM = (mtp_socket *)shmat(m_sm_shmid, (void *)0, 0);

    m_pop.sem_num = 0;
    semop(m_sm_mutex, &m_pop, 1); // wait on m_sm_mutex
    for (i = 0; i < MAX_SOCKETS; i++)
    {
        if (m_SM[i].is_free == 0 && m_SM[i].pid == pid)
        {
            if (m_debug) {
                printf("Socket ID: %d\n", i);
                printf("UDP Socket ID: %d\n", m_SM[i].udp_sock);
                printf("Source IP: %s\n", m_SM[i].source_ip);
                printf("Source Port: %d\n", m_SM[i].source_port);
                printf("Destination IP: %s\n", m_SM[i].dest_ip);
                printf("Destination Port: %d\n", m_SM[i].dest_port);
                printf("\n");
            }
        }
    }
    m_vop.sem_num = 0;
    semop(m_sm_mutex, &m_vop, 1); // signal m_sm_mutex
    shmdt(m_SM);
    return;
}

int dropMessage(float p)
{
    float r = (float)rand() / (float)(RAND_MAX);
    if (r < p)
    {
        return 1;
    }
    return 0;
}