#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/socket.h>
#include <linux/in.h>
#include <unistd.h>
#include <string.h>
#include <stdbool.h>
#include <limits.h>


#define MAX 200
int msgReceived[5] = {-1, -1, -1, -1, -1};
int serverMsgReceived[7] = {-1, -1, -1, -1, -1, -1, -1};
int socks[7];
bool breakFlag = false;
bool breakSerFlag = false;
int totMsgSent = 0;
int totMsgRcvd = 0;


typedef struct
{
    int sock;
    struct sockaddr address;
    int addr_len;
} connection_t;

bool isLocked = false;
int serverNum;
int ports[] = {8080 ,8081, 8082, 8083, 8084, 8085, 8086};
char *hostnames[] = {"10.176.69.32", "10.176.69.33", "10.176.69.34", "10.176.69.35", "10.176.69.36", "10.176.69.37", "10.176.69.38"};

// int connect_to_server(char *hostname, int port)
// {
//     int sock = -1;
//     struct sockaddr_in address;
//     struct hostent *host;

//     /* create socket */
//     sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
//     if (sock <= 0)
//     {
//         fprintf(stderr, "%s: error: cannot create socket\n", hostname);
//         return -1;
//     }

//     /* connect to server */
//     address.sin_family = AF_INET;
//     address.sin_port = htons(port);
//     host = gethostbyname(hostname);
//     if (!host)
//     {
//         fprintf(stderr, "%s: error: unknown host %s\n", hostname, hostname);
//         return -1;
//     }
//     memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
//     if (connect(sock, (struct sockaddr *)&address, sizeof(address)))
//     {
//         fprintf(stderr, "%s: error: cannot connect to host %s\n", hostname, hostname);
//         return -1;
//     }

//     return sock;
// }

void *process(void *ptr)
{
    printf("In conn process start\n");
    char buffer[1000];
    int len, n, rsp_msg_len;
    connection_t *conn;
    long addr = 0;
    char resp_msg[1000];

    if (!ptr)
    {
        printf("Exiting\n");
        pthread_exit(0);
    }

    conn = (connection_t *)ptr;

    len = 0;

    // printf("Waiting for message\n");
    read(conn->sock, &len, sizeof(int));
    
    if (len > 0)
    {
        // printf("trying to read %d bytes\n", len);
        breakFlag = false;
        int ctr = 0;
        buffer[len] = '\0';
        read(conn->sock, buffer, len);
        printf("Received Msg:%s\n", buffer);
        
        char *p = strtok(buffer, ":");
        char *array[6];
        int i = 0;
        while (p != NULL && i < 6)
        {
            array[i++] = p;
            p = strtok(NULL, ":");
        }

        printf("Client %s terminated: Total messages sent: %s and Total messages received: %s", array[1], array[3], array[5]);
        msgReceived[atoi(array[1])] = 1;
        // close(conn->sock);
        // free(conn);
        printf("\n");
        pthread_exit(0);
    }
    
}

/*void *closeServers(void *socket)
{
    int sock = *(int *)socket;
    char message[1000];
    char msg[1000];
    int len;
    int ctr = 0;
    strcpy(message, "TERMINATE:0:0");

    len = strlen(message);
    write(sock, &len, sizeof(int));
    write(sock, message, len);
    
    len = 0;
    if (read(sock, &len, sizeof(int)) <= 0)
    {
        printf("%d\n", len);
        printf("Error: Failed to read message length from server %d\n", 1);
        pthread_exit(0);
    }
    else
    {
        // printf("Trying to read %d bytes\n", msg_len);
        if (read(sock, msg, len) <= 0)
        {
            printf("Error: Failed to read message from server %d\n", 1);
            pthread_exit(0);
        }
        else
        {
            msg[len] = '\0';
            ctr = 0;
            breakSerFlag = false;
            
            char *p = strtok(msg, ":");
            char *array[6];
            int i = 0;
            while (p != NULL && i < 6)
            {
                array[i++] = p;
                p = strtok(NULL, ":");
            }

            printf("Server %s terminated: Total messages sent: %s and Total messages received: %s", array[1], array[3], array[5]);
            serverMsgReceived[atoi(array[1])] = 1;

            for (int i = 0; i < 7; i++) {
                if (serverMsgReceived[i] == 1) {
                    ctr++;
                }
            }

            if (ctr == 7) {
                breakSerFlag = true;
                pthread_exit(0);
            }

        }
    }

    pthread_exit(0);
}*/

int main(int argc, char **argv)
{
    int sock = -1;
    struct sockaddr_in address;
    int port;
    connection_t *connection;
    pthread_t threads[5];
    pthread_t send_req[7];
    int i = 0;

    /* check for command line arguments */
    if (argc != 3)
    {
        fprintf(stderr, "usage: %s port\n", argv[0]);
        return -1;
    }

    /* obtain port number */
    if (sscanf(argv[1], "%d", &port) <= 0)
    {
        fprintf(stderr, "%s: error: wrong parameter: port\n", argv[0]);
        return -2;
    }

    // Get server name
    if (sscanf(argv[2], "%d", &serverNum) <= 0)
    {
        fprintf(stderr, "%s: error: wrong servername: serverName\n", argv[1]);
        return -2;
    }

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0)
    {
        fprintf(stderr, "%s: error: cannot create socket\n", argv[0]);
        return -3;
    }

    /* bind socket to port */
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(port);
    if (bind(sock, (struct sockaddr *)&address, sizeof(struct sockaddr_in)) < 0)
    {
        fprintf(stderr, "%s: error: cannot bind socket to port %d\n", argv[0], port);
        return -4;
    }

    /* listen on port */
    if (listen(sock, 5) < 0)
    {
        fprintf(stderr, "%s: error: cannot listen on port\n", argv[0]);
        return -5;
    }

    printf("%s: ready and listening\n", argv[0]);

    while (1)
    {
        /* accept incoming connections */
        connection = (connection_t *)malloc(sizeof(connection_t));
        connection->sock = accept(sock, &connection->address, &connection->addr_len);
        if (connection->sock <= 0)
        {
            free(connection);
        }
        else
        {
            /* start a new thread but do not wait for it */
            printf("Creating thread\n");
            pthread_create(&threads[i], 0, process, (void *)connection);
            // pthread_detach(thread);
            i++;
        }
    }



    // if (breakFlag) {
    //     for (int i = 0; i < 7; i++)
    //     {
    //         socks[i] = connect_to_server(hostnames[i], ports[i]);
    //         if (socks[i] < 0)
    //         {
    //             return -1;
    //         }
    //     }

    //     for (int i = 0; i < 7; i++)
    //     {
    //         pthread_create(&send_req[i], NULL, closeServers, (void *)&socks[i]);
    //     }
    // }
    printf("Abcd");
    for (int i = 0; i < 5; i++)
    {
        pthread_join(threads[i], NULL);
    }
    printf("Terminating S0");
    return 0;
}
// gcc -o master masterServer.c -lpthread -std=c99