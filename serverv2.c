#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <sys/select.h>
#include <pthread.h>
#define INTERVAL 1
#define NUM_CLIENTS 10
#define DISCONNECTED_SIGNAL -9999
#define DATA_SIGNAL -9998

char buffer[1025];
int fdsForkToMain[2];
int fdsMainToFork[2][NUM_CLIENTS];
int fdsMainToForkFree[NUM_CLIENTS] = {0};
int clientfd;
int i;
int clientId;

/**
 * Run by server main 
 * Objective: Receive data from one 
 * server fork and transfer them to other servers fork
 * 
 */
void *passData()
{
    while (1)
    {
        int fromClientId;
        int signal;
        if (read(fdsForkToMain[0], &signal, sizeof(int)) > 0)
        {
            read(fdsForkToMain[0], &fromClientId, sizeof(int));
            switch (signal)
            {
            case DATA_SIGNAL:
                // Got data from Server Fork
                read(fdsForkToMain[0], &buffer, sizeof(buffer));
                printf("Server Main: Got data from client %d\n", fromClientId);
                printf("Server Main: Got: %s", buffer);
                for (i = 0; i < NUM_CLIENTS; i++)
                {
                    if (fdsMainToForkFree[i] && i != fromClientId)
                    {
                        printf("Server Main: Send data to server fork %d\n", i);
                        write(fdsMainToFork[i][1], buffer, sizeof(buffer));
                    }
                }
                break;
            case DISCONNECTED_SIGNAL:
                // Server Fork is shuting down due to client disconnected
                fdsMainToForkFree[fromClientId] = 0;
                printf("Server Main: Client %d disconnected\n", fromClientId);
                break;
            default:
                printf("Server Main: Wrong Signal Received\n");
            }
        }

        sleep(INTERVAL);
    }
    return NULL;
}

/**
 * Run by server fork 
 * Objective: Receive data from Server Main,
 * Then send the data to their corresponding client
 */
void *forkWriteDataToClient()
{

    while (1)
    {

        // for (i = 0; i < NUM_CLIENTS; i++)
        // {
        if (read(fdsMainToFork[clientId][0], &buffer, sizeof(buffer)) > 0)
        {
            printf("Server fork: Got: %s", buffer);
            printf("Server fork: Send data to client %d\n", clientId);
            write(clientfd, buffer, sizeof(buffer));
            // break;
        }
        // }

        sleep(INTERVAL);
    }
    return NULL;
}

int main()
{
    int sockfd;

    pipe(fdsForkToMain);

    // for (i = 0; i < NUM_CLIENTS; i++)
    // {
    //     pipe(fdsMainToFork[i]);
    // }
    // pipe(fdsMainToFork);

    // create new socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0)
    {
        perror("create socket failed");
        exit(0);
    }

    // set reusing address
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));

    // make the server non-blocking
    int fl = fcntl(sockfd, F_GETFL, 0);
    fl |= O_NONBLOCK;
    fcntl(sockfd, F_SETFL, fl);

    struct sockaddr_in serverAddress;
    serverAddress.sin_family = AF_INET;
    serverAddress.sin_addr.s_addr = htonl(INADDR_ANY);
    serverAddress.sin_port = htons(8784);

    // bind socket to port
    if (bind(sockfd, (struct sockaddr *)&serverAddress, sizeof(serverAddress)) < 0)
    {
        perror("bind socket failed");
        exit(0);
    }

    pthread_t tid;

    pthread_create(&tid, NULL, passData, NULL);

    listen(sockfd, 10);

    while (1)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        int maxfd = sockfd;

        printf("Waiting for connection\n");
        select(maxfd + 1, &set, NULL, NULL, NULL);

        clientfd = accept(sockfd, (struct sockaddr *)NULL, NULL);

        for (i = 0; i < NUM_CLIENTS; i++)
        {
            if (fdsMainToForkFree[i] == 0)
            {
                fdsMainToForkFree[i] = 1;
                clientId = i;
                break;
            }
        }
        pipe(fdsMainToFork[clientId]);

        if (fork() == 0)
        {
            // child
            fd_set clientSet;
            FD_ZERO(&clientSet);

            printf("Server fork: client %d has connected.\n", clientId);

            // make it non blocking
            int fl = fcntl(clientfd, F_GETFL, 0);
            fl |= O_NONBLOCK;
            fcntl(clientfd, F_SETFL, fl);

            FD_SET(clientfd, &clientSet);

            pthread_t childTid;
            pthread_create(&childTid, NULL, forkWriteDataToClient, NULL);

            while (1)
            {
                select(clientfd + 1, &clientSet, NULL, NULL, NULL);

                if (read(clientfd, buffer, sizeof(buffer)) > 0)
                {
                    // Client send data
                    int dataSignal = DATA_SIGNAL;
                    write(fdsForkToMain[1], &dataSignal, sizeof(int));
                    write(fdsForkToMain[1], &clientId, sizeof(int));
                    write(fdsForkToMain[1], buffer, sizeof(buffer));
                }
                else
                {
                    // Client disconnect
                    printf("Server Fork: Client %d disconnected\n", clientId);
                    printf("Server Fork: Send DISCONNECTED_SIGNAL\n");
                    int disconnectedSignal = DISCONNECTED_SIGNAL;
                    write(fdsForkToMain[1], &disconnectedSignal, sizeof(int));
                    write(fdsForkToMain[1], &clientId, sizeof(int));
                    printf("Server Fork: Exited\n");
                    exit(0);
                }
                sleep(INTERVAL);
            }
        }
    }
}