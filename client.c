#include <stdio.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <pthread.h>
#include <sys/select.h>

int clientName;
int criticalStateReached = 0;
bool inCriticalSection = false;
char clientId[20];
char tymStr[1000];
int socks[7];
bool requestsSent = false;
int reqSent[8] = {0, 0, 0, 0, 0, 0,0,0};
int msgRcvd[8] = {0, 0, 0, 0, 0, 0,0,0};
int grantReceived[8] = {0, 0, 0, 0, 0, 0,0,0};
int ports[] = {8080 ,8081, 8082, 8083, 8084, 8085, 8086};
char *hostnames[] = {"10.176.69.32", "10.176.69.33", "10.176.69.34", "10.176.69.35", "10.176.69.36", "10.176.69.37", "10.176.69.38"};
int totalMessagesSent = 0;
int totalMessagesReceived = 0;
int totMsgPerCS = 0;
int ctr = 0;

int num_servers = 7;
int socCreation;
time_t reqStartTime;
bool isQuorum = false;
// time_t csReached;

struct thread_args
{
    int socket;
    int id;
    int req_type;
};

struct rec_thread_args {
    int socket;
    int id;
};
struct thread_args thread_data_array[7];
struct rec_thread_args rec_thread_data_array[7];


// Establish connections to server
int connect_to_server(char *hostname, int port)
{
    int sock = -1;
    struct sockaddr_in address;
    struct hostent *host;

    /* create socket */
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock <= 0)
    {
        fprintf(stderr, "%s: error: cannot create socket\n", hostname);
        return -1;
    }

    /* connect to server */
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    host = gethostbyname(hostname);
    if (!host)
    {
        fprintf(stderr, "%s: error: unknown host %s\n", hostname, hostname);
        return -1;
    }
    memcpy(&address.sin_addr, host->h_addr_list[0], host->h_length);
    if (connect(sock, (struct sockaddr *)&address, sizeof(address)))
    {
        fprintf(stderr, "%s: error: cannot connect to host %s\n", hostname, hostname);
        return -1;
    }

    return sock;
}

void *sendData(void *t_args)
{
    // printf("In client conn process start\n");
    struct thread_args *my_data;
    my_data = (struct thread_args *)t_args;
    int sock = my_data->socket;
    char message[1000];
    int len;

    if (my_data -> req_type == 0) {
        printf("Sending REQUEST to server %d from client %d\n", my_data->id, clientName);
        sprintf(message, "REQUEST:%d:%ld", clientName, time(0));
        ctr++;
    } else if (my_data -> req_type == 1) {
        printf("Sending RELEASE to server %d from client\n", my_data->id, clientName);
        sprintf(message, "RELEASE:%d:%ld", clientName, time(0));
    }

    len = strlen(message);
    write(sock, &len, sizeof(int));
    write(sock, message, len);
    totalMessagesSent++;
    totMsgPerCS++;

    pthread_exit(0);
}

void *recData(void *rt_args)
{
    char msg[1000];
    struct rec_thread_args *rec_thread_data;
    rec_thread_data = (struct rec_thread_args *)rt_args;
    int sock = rec_thread_data->socket;
    int sId = rec_thread_data->id;
    printf("Created listner Thread for %d\n", sId);
    while (1) {
        int msg_len = 0;
        memset( msg, '\0', sizeof(msg));
        // printf("Waiting to read at %d\n", clientName);
        if (read(sock, &msg_len, sizeof(int)) <= 0)
        {
            printf("%d\n", msg_len);
            printf("Error: Failed to read message length from server %d\n", 1);
            pthread_exit(0);
        }
        else
        {
            // printf("Trying to read %d bytes\n", msg_len);
            if (read(sock, msg, msg_len) <= 0)
            {
                printf("Error: Failed to read message from server %d\n", 1);
                pthread_exit(0);
            }
            else
            {
                msg[msg_len] = '\0';
                
                
                char *p = strtok(msg, ":");
                char *array[3];
                int i = 0;
                while (p != NULL && i < 3)
                {
                    array[i++] = p;
                    p = strtok(NULL, ":");
                }
                // printf("Grant req time %d for a req %d with quorum %d", atoi(array[2]), reqStartTime, isQuorum);
                if ((atoi(array[2]) >= reqStartTime) && (!isQuorum)) {
                    printf("Received %s from server: %d\n", array[0], atoi(array[1]));
                    if (strcmp(array[0], "GRANT") == 0) {
                        grantReceived[atoi(array[1])] = 1;
                    }
                    totalMessagesReceived++;
                    totMsgPerCS++;
                    ctr++;

                }

                msgRcvd[sId] += 1;
                
                if ( msgRcvd[sId]>= 20) {
                    printf("Quiting for %d\n",sId);
                    pthread_exit(0);
                }
                msg[msg_len] = '\0';
            }
        }
    }

    pthread_exit(0);
}

int create_socks() {
    // Connect to all servers
    for (int i = 0; i < num_servers; i++)
    {
        socks[i] = connect_to_server(hostnames[i], ports[i]);
        if (socks[i] < 0)
        {
            return -1;
        }
    }
}

void close_sockets() {
    // Close connections to all servers
    for (int i = 0; i < num_servers; i++)
    {
        close(socks[i]);
    }
}

int checkQuorum (int index) {
    // printf("%d->",index);
    if (index * 2 > 8 || (index * 2 + 1) > 8) {
        return grantReceived[index];
    }
     
    if (grantReceived[index]) {
        int l = checkQuorum(2 * index);
        int r = checkQuorum(2*index + 1);
        return l || r;
    }else {
        int left = checkQuorum(2 * index);
        int right = checkQuorum(2*index + 1);
        return left && right;
    }
}

int main(int argc, char **argv)
{
    int port;
    int sock = -1;
    struct sockaddr_in address;
    struct hostent *host;
    int len;
    char message[1000];
    // time_t currT;


    pthread_t send_req[num_servers];
    pthread_t rec_req[num_servers];

    // Get Client name
    if (sscanf(argv[1], "%d", &clientName) <= 0)
    {
        fprintf(stderr, "%s: error: wrong servername: serverName\n", argv[1]);
        return -2;
    }

    socCreation = create_socks();
    if (socCreation == -1) {
        return -1;
    }

    // Creating threads that constantly listen for server messages
    for (int i = 0; i < num_servers; i++)
    {
        rec_thread_data_array[i].socket = socks[i];
        rec_thread_data_array[i].id = i+1;
        pthread_create(&rec_req[i], NULL, recData, (void *)&rec_thread_data_array[i]);
    }

   while (criticalStateReached < 20)
    {
        // Create connections and send requests only once per iteration
        if (!requestsSent)
        {  
            int random_interval = rand() % 5 + 5;
            printf("Sleeping for %d before sending requests\n", random_interval);
            sleep(random_interval);
            reqStartTime = time(0);
            // Send request to each server using separate threads
            for (int i = 0; i < num_servers; i++)
            {
                thread_data_array[i].socket = socks[i];
                thread_data_array[i].id = i+1;
                thread_data_array[i].req_type = 0;
                pthread_create(&send_req[i], NULL, sendData, (void *)&thread_data_array[i]);
            }
            requestsSent = true;
        }
        // printf("\n");
        if (checkQuorum(1)) {
            isQuorum = true;
            printf("entering %d\n", clientName);
            printf("Execution No: %d\n", ++criticalStateReached);
            printf("Quorum reached with: ");
            for (int i = 0; i < 8; i++) {
                if (grantReceived[i] == 1) {
                    printf("%d ", i);
                }
            }
            printf("\n");
            
            printf("Curr Time: %ld\n", time(0));
            printf("Time taken to reach critical section: %d\n", time(0) - reqStartTime);
            printf("Total messages exchanged: %d\n", ctr);
            // totMsgPerCS = 0;
            ctr = 0;
            sleep(3);
            
            // close all req threads
            printf("Closing all threads\n");
            for (int i = 0; i < num_servers; i++)
            {
                pthread_join(send_req[i], NULL);
            }

            memset(reqSent, 0, sizeof(reqSent));
            memset(grantReceived, 0, sizeof(grantReceived));

            // Loop over the thread_data_array and set req_type to 1
            for (int i = 0; i < 7; i++) {
                thread_data_array[i].socket = socks[i];
                thread_data_array[i].id = i+1;
                thread_data_array[i].req_type = 1;
            }
            // Create new threads to send release messages
            for (int i = 0; i < num_servers; i++) {
                if (pthread_create(&send_req[i], NULL, sendData, &thread_data_array[i]) != 0) {
                    fprintf(stderr, "Error: Failed to create release thread %d\n", i);
                }
            }

            // close all threads
            printf("Closing all release threads\n");
            for (int i = 0; i < num_servers; i++)
            {
                pthread_join(send_req[i], NULL);
            }
            
            requestsSent = false;
            isQuorum = false;
        }

    }

    for (int i = 0; i < num_servers; i++)
    {
        pthread_join(rec_req[i], NULL);
    }

    close_sockets();
    printf("Execution Done\n");
    printf("Connecting to S0\n");

    // Send completion info
    char completionMessage[1000];
    int S0Socket = connect_to_server("10.176.69.45", 8087);
    int comMsgLen = 0;

    sprintf(completionMessage, "CLIENT:%d:SENT:%d:RECEIVED:%d",clientName, totalMessagesSent, totalMessagesReceived);
    comMsgLen = strlen(completionMessage);
    write(S0Socket, &comMsgLen, sizeof(int));
    write(S0Socket, completionMessage, comMsgLen);
    printf("Terminating");
    close(S0Socket);
    return 0;
}
//gcc -o client client.c -lpthread -std=c99