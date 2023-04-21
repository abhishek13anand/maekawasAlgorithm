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
int socket_val[6] = {-1, -1, -1, -1, -1, -1};
int totMsgSent = 0;
int totMsgRcvd = 0;

typedef struct
{
    int value;
    int priority;
} Node;

Node heap[MAX];
int size = 0;

void swap(Node *a, Node *b)
{
    Node temp = *a;
    *a = *b;
    *b = temp;
}

void bubble_up(int index)
{
    if (index == 0)
        return;

    int parent = (index - 1) / 2;
    if (heap[parent].priority > heap[index].priority)
    {
        swap(&heap[parent], &heap[index]);
        bubble_up(parent);
    }
}

void bubble_down(int index)
{
    int left_child = (2 * index) + 1;
    int right_child = (2 * index) + 2;
    int min_index = index;

    if (left_child < size && heap[left_child].priority < heap[min_index].priority)
    {
        min_index = left_child;
    }

    if (right_child < size && heap[right_child].priority < heap[min_index].priority)
    {
        min_index = right_child;
    }

    if (min_index != index)
    {
        swap(&heap[index], &heap[min_index]);
        bubble_down(min_index);
    }
}

void push(int value, int priority)
{
    if (size == MAX)
    {
        printf("Error: Heap overflow\n");
        return;
    }

    heap[size].value = value;
    heap[size].priority = priority;
    bubble_up(size);
    size++;
}

Node pop()
{
    if (size == 0)
    {
        Node empty_node = {0, 0};
        return empty_node;
    }

    Node min_node = heap[0];
    heap[0] = heap[size - 1];
    size--;
    bubble_down(0);

    return min_node;
}

void delete_value(int value)
{
    int i;
    for (i = 0; i < size; i++)
    {
        if (heap[i].value == value)
        {
            heap[i] = heap[size - 1];
            size--;
            i--;
        }
    }

    for (i = size / 2 - 1; i >= 0; i--)
    {
        bubble_down(i);
    }
}

typedef struct
{
    int sock;
    struct sockaddr address;
    int addr_len;
} connection_t;

bool isLocked = false;
int serverNum;

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

    while (true)
    {
        len = 0;

        // printf("Waiting for message\n");
        read(conn->sock, &len, sizeof(int));
        
        if (len > 0)
        {
            // printf("trying to read %d bytes\n", len);
            buffer[len] = '\0';
            read(conn->sock, buffer, len);
            
            printf("Received Msg:%s\n", buffer);
            totMsgRcvd++;
            char *p = strtok(buffer, ":");
            char *array[3];
            int i = 0;
            while (p != NULL && i < 3)
            {
                array[i++] = p;
                p = strtok(NULL, ":");
            }

            // get client Id anf client socket
            int cId = atoi(array[1]);
            int tStamp = atoi(array[2]);

            if (strcmp(array[0], "REQUEST") == 0)
            {
                // printf("received Client %d has socket %d\n", cId, conn->sock);
                
                socket_val[cId] = conn->sock;
                if (isLocked)
                {
                    printf("Pushing client %d in Q of %d\n", cId, serverNum);
                    push(cId, tStamp);
                }
                else
                {
                    printf("Sending grant to client: %d from server: %d\n", cId, serverNum);
                    sprintf(resp_msg, "GRANT:%d:%d", serverNum, tStamp);
                    // printf("Msg to be sent: %s", resp_msg);
                    rsp_msg_len = strlen(resp_msg);
                    write(socket_val[cId], &rsp_msg_len, sizeof(int));
                    // printf("Trying to write %d bytes\n", rsp_msg_len);
                    n = write(socket_val[cId], resp_msg, strlen(resp_msg));
                    totMsgSent++;
                    if (n < 0)
                    {
                        fprintf(stderr, "Error writing to socket.\n");
                        // free(buffer);
                        break;
                    }
                    isLocked = true;
                }
            }
            else if (strcmp(array[0], "RELEASE") == 0)
            {
                isLocked = false;
                delete_value(cId);
                if (size > 0)
                {
                    Node head_node = pop();
                    printf("Poping %d from Q in server %d\n", head_node.value,  serverNum);

                    printf("Sending grant to client: %d from server: %d\n", head_node.value, serverNum);
                    sprintf(resp_msg, "GRANT:%d:%d", serverNum, head_node.priority);
                    // printf("Msg to be sent: %s", resp_msg);clear
                    rsp_msg_len = strlen(resp_msg);

                    write(socket_val[head_node.value], &rsp_msg_len, sizeof(int));
                    n = write(socket_val[head_node.value], resp_msg, strlen(resp_msg));
                    totMsgSent++;
                    if (n < 0)
                    {
                        fprintf(stderr, "Error writing to socket.\n");
                        // free(buffer);
                        break;
                    }
                    isLocked = true;
                }
                else
                {
                    isLocked = false;
                }
            }


            // int rsp_msg_len = strlen(resp_msg);
            // write(conn->sock, &rsp_msg_len, sizeof(int));
            // n = write(conn->sock, resp_msg, strlen(resp_msg));
            // if (n < 0)
            // {
            //     fprintf(stderr, "Error writing to socket.\n");
            //     // free(buffer);
            //     break;
            // }
        }
    }

   /* while (false)
    {
        len = 0;

        // read length of message 
        // printf("Waiting for message\n");
        read(conn->sock, &len, sizeof(int));

        if (len > 0)
        {
            printf("Trying to read %d bytes\n", len);
            buffer[len] = '\0';
            read(conn->sock, buffer, len);

            printf("Message Read: %s\n", buffer);
            sprintf(resp_msg, "Server %d is alive", serverNum);

            int rsp_msg_len = strlen(resp_msg);
            printf("Write Start: %s\n", resp_msg);
            write(conn->sock, &rsp_msg_len, sizeof(int));

            printf("Trying to write %d bytes\n", rsp_msg_len);
            n = write(conn->sock, resp_msg, strlen(resp_msg));
            if (n < 0)
            {
                fprintf(stderr, "Error writing to socket.\n");
                // free(buffer);
                break;
            }
            // char *p = strtok(buffer, ":");
            // char *array[3];
            // int i = 0;
            // while (p != NULL && i < 3)
            // {
            //     array[i++] = p;
            //     p = strtok(NULL, ":");
            // }

            // // get client Id anf client socket
            // int cId = atoi(array[1]);
            // int tStamp = atoi(array[2]);
            
            // if (strcmp(array[0], "REQUEST") == 0)
            // {
            //     socket_val[cId] = conn->sock;
            //     if (isLocked)
            //     {
            //         printf("Pushing client %d in Q of %d\n", cId, serverNum);
            //         push(cId, tStamp);
            //     }
            //     else
            //     {
            //         printf("Sending grant to client: %d from server: %d\n", cId, serverNum);
            //         sprintf(resp_msg, "GRANT:%d", serverNum);
                    
            //         int rsp_msg_len = strlen(resp_msg);
            //         write(socket_val[cId], &rsp_msg_len, sizeof(int));
            //         printf("Trying to write %d bytes\n", rsp_msg_len);
            //         n = write(socket_val[cId], resp_msg, strlen(resp_msg));
            //         if (n < 0)
            //         {
            //             fprintf(stderr, "Error writing to socket.\n");
            //             // free(buffer);
            //             break;
            //         }
            //         isLocked = true;
            //     }
            // }
            // else if (strcmp(array[0], "RELEASE") == 0)
            // {
            //     isLocked = false;
            //     delete_value(cId);
            //     if (size > 0)
            //     {
            //         printf("Poping from Q in server %d\n", serverNum);
            //         Node head_node = pop();
                    
            //         printf("Sending grant to client: %d from server: %d\n", head_node.value, serverNum);
            //         sprintf(resp_msg, "GRANT:%d", serverNum);

            //         int rsp_msg_len = strlen(resp_msg);
            //         write(socket_val[head_node.value], &rsp_msg_len, sizeof(int));
            //         n = write(socket_val[head_node.value], resp_msg, strlen(resp_msg));
            //         if (n < 0)
            //         {
            //             fprintf(stderr, "Error writing to socket.\n");
            //             // free(buffer);
            //             break;
            //         }
            //         isLocked = true;
            //     }
            //     else
            //     {
            //         isLocked = false;
            //     }
            // }

            // /* check for end of transmission 
            if (strcmp(buffer, "DONE") == 0)
            {
                // free(buffer);
                break;
            }

            /* free buffer 
            // free(buffer);
        }
        break;
    }*/

    /* close socket and clean up */
    close(conn->sock);
    free(conn);
    pthread_exit(0);
}

int main(int argc, char **argv)
{
    int sock = -1;
    struct sockaddr_in address;
    int port;
    connection_t *connection;
    pthread_t thread;

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
            pthread_create(&thread, 0, process, (void *)connection);
            pthread_detach(thread);
        }
    }

    return 0;
}
// gcc -o server server.c -lpthread