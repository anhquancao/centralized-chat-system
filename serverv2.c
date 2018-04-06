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
#define NUM_CLIENTS 1

char buffer[1025];
int fdsForkToMain[2];
int fdsMainToFork[2][4];
// int fdsMainToForkFree[4] = {0};
int clientfd;
int i;
int clientId;

/**
 * Run by server main to transfer data to servers fork
 * 
 */
void *passData()
{
    while (1)
    {
        if (read(fdsForkToMain[0], &buffer, sizeof(buffer)) > 0)
        {
            printf("Server Main got: %s", buffer);
            for (i = 0; i < NUM_CLIENTS; i++)
            {
                write(fdsMainToFork[i][1], buffer, sizeof(buffer));
            }
        }
        sleep(INTERVAL);
    }
    return NULL;
}

/**
 * Run by server fork to write data to client
 * 
 */
void *forkWriteDataToClient()
{

    while (1)
    {

        for (i = 0; i < NUM_CLIENTS; i++)
        {
            if (read(fdsMainToFork[i][0], &buffer, sizeof(buffer)) > 0)
            {
                printf("Client %d's Server Fork got: %s", clientfd, buffer);
                printf("Send data to client: %d\n", clientfd);
                write(clientfd, buffer, sizeof(buffer));
                // break;
            }
        }

        sleep(INTERVAL);
    }
    return NULL;
}

int main()
{
    int sockfd;

    pipe(fdsForkToMain);

    for (i = 0; i < NUM_CLIENTS; i++)
    {
        pipe(fdsMainToFork[i]);
    }
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

        if (fork() == 0)
        {

            fd_set clientSet;
            FD_ZERO(&clientSet);

            printf("Server fork: client %d has connected.\n", clientfd);

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
                    write(fdsForkToMain[1], buffer, sizeof(buffer));
                }
                sleep(INTERVAL);
            }

            exit(0);
            // child
        }
        else
        {
            // parent
        }
    }
}