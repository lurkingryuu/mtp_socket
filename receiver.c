#include <stdio.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <ifaddrs.h>
#include <getopt.h>
#include <msocket.h>

#define MESSAGE_SIZE 1024
int INF = 1e4;
int sfd;
int fd;
int debug = 0;
int PORT = -1;
char *ADDR = "";
int OTHER_PORT = -1;
char *OTHER_ADDR = "";
void set_addr();
void parse_args(int argc, char *argv[]);

char *filename = NULL;
void sigint_handler(int signum)
{
    m_close(sfd);
    close(fd);
    exit(signum);
}

int main(int argc, char *argv[])
{
    // ----------------- Parse command line arguments ----------------- //
    signal(SIGINT, sigint_handler);
    parse_args(argc, argv);

    // ------------------------- Main code ------------------------- //
    int fd = open(filename, O_CREAT | O_WRONLY | O_TRUNC, 0777);
    if (fd < 0)
    {
        pperror("open");
        sigint_handler(-1);
    }

    sfd = m_socket(AF_INET, SOCK_MTP, 0);
    if (sfd < 0)
    {
        pperror("socket");
        sigint_handler(-1);
    }
    ppgreen("Socket created\n");
    fflush(stdout);

    if (m_bind(sfd, ADDR, PORT, OTHER_ADDR, OTHER_PORT) < 0)
    {
        perror("bind");
        sigint_handler(-1);
    }
    printf(GREEN "Bound to %s:%d -> %s:%d\n" RESET, ADDR, PORT, OTHER_ADDR, OTHER_PORT);
    prinfo();

    struct sockaddr_in other_addr;
    other_addr.sin_family = AF_INET;
    other_addr.sin_port = htons(OTHER_PORT);
    other_addr.sin_addr.s_addr = inet_addr(OTHER_ADDR);

    char buff[1024];
    int c = 0, msg_num = 0;
    while (1)
    {
        int len = sizeof(other_addr);
        int rlen = -1;
        c = 0;
        while (rlen < 0)
        {
            rlen = m_recvfrom(sfd, buff, MESSAGE_SIZE, 0, (struct sockaddr *)&other_addr, &len);
            if (rlen < 0)
            {
                // both of the statements combined give a max timeout of 700 seconds
                usleep(70000);
                c++;
            }
            if (c > INF)
            {
                pperror("Connection timed out\n");
                sigint_handler(0);
            }
        }
        if (buff[0] == '$')
        {
            ppblue("Received EOF\n");
            break;
        }
        rlen = strlen(buff);

        printf(GREEN "Received Message %d\n" RESET, ++msg_num);
        if (debug)
        {
            printf("-----------------------------\n");
            buff[rlen] = '\0';
            printf(GREEN "Received:" RESET " %s\n", buff);
            printf("-----------------------------\n");
        }
        write(fd, buff, rlen);
    }

    ppmagenta("File received successfully\n");
    ppmagenta("Press Enter to exit\n");
    getchar();

    sigint_handler(0);
}

// ---------------- Helper Functions ---------------- //
void set_addr()
{
    // Retrieve IP addresses
    struct ifaddrs *ifaddr, *ifa;
    char **addrs;
    int n = getifaddrs(&ifaddr);
    if (n == -1)
    {
        perror("getifaddrs");
        exit(1);
    }

    // Print the IP addresses
    printf("Available IP addresses:\n");
    int i = 1;
    addrs = malloc(sizeof(char *));
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next)
    {
        if (ifa->ifa_addr != NULL && ifa->ifa_addr->sa_family == AF_INET)
        {
            addrs = realloc(addrs, sizeof(char *) * i);
            addrs[i - 1] = malloc(sizeof(char) * strlen(inet_ntoa(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr)));
            strcpy(addrs[i - 1], inet_ntoa(((struct sockaddr_in *)ifa->ifa_addr)->sin_addr));
            printf("%d: %s\n", i, addrs[i - 1]);
            i++;
        }
    }
    printf("Enter your choice: ");
    int choice;
    scanf("%d", &choice);
    if (choice < 1 || choice >= i)
    {
        printf("Invalid choice\n");
        exit(1);
    }
    ADDR = malloc(sizeof(char) * strlen(addrs[choice - 1]));
    strcpy(ADDR, addrs[choice - 1]);
    free(addrs);
    freeifaddrs(ifaddr);
}

void parse_args(int argc, char *argv[])
{
    // d: debug, h: host, p: port, H: server host, P: server port
    int opt;
    while ((opt = getopt(argc, argv, "dh:p:H:P:f:")) != -1)
    {
        switch (opt)
        {
        case 'd':
            debug = 1;
            break;
        case 'h':
            ADDR = optarg;
            break;
        case 'p':
            PORT = atoi(optarg);
            break;
        case 'H':
            OTHER_ADDR = optarg;
            break;
        case 'P':
            OTHER_PORT = atoi(optarg);
            break;
        case 'f':
            filename = optarg;
            break;
        default:
            printf("Usage: %s [-d] [-h host] [-p port] [-H other_host] [-P other_port]\n", argv[0]);
            exit(1);
        }
    }
    if (strcmp(ADDR, "") == 0)
    {
        set_addr();
    }
    if (PORT == -1)
    {
        printf("Enter port: ");
        scanf("%d", &PORT);
    }
    if (strcmp(OTHER_ADDR, "") == 0)
    {
        char *OTHER_ADDR = malloc(16);
        printf("Enter other address: ");
        scanf("%s", OTHER_ADDR);
        OTHER_ADDR = OTHER_ADDR;
    }
    if (OTHER_PORT == -1)
    {
        printf("Enter other port: ");
        scanf("%d", &OTHER_PORT);
    }
    if (filename == NULL)
    {
        printf("Enter filename to save the received file: ");
        scanf("%s", filename);
    }
}