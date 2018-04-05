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

int clientfds;
char buffer[1025];
int fds[2];

void closeAll()
{
    close(fds[0]);
    close(fds[1]);
}

void *writeToClient()
{
    while (1)
    {
        for (int i = 0; i < 100; i++)
        {
            // if (clientfds[i] > 0)
            // {
            //     // printf("Send to %d: \n", clientfds[i]);
            //     fgets(buffer, sizeof(buffer), stdin);
            //     write(clientfds[i], buffer, sizeof(buffer));
            // }
        }
    }
    return NULL;
}

int main()
{
    int sockfd;

    pipe(fds);

    memset(&clientfds, 0, sizeof(clientfds));

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

    // pthread_t tid;

    // pthread_create(&tid, NULL, writeToClient, NULL);

    listen(sockfd, 10);

    while (1)
    {
        fd_set set;
        FD_ZERO(&set);
        FD_SET(sockfd, &set);
        int maxfd = sockfd;

        printf("Waiting for connection\n");
        select(maxfd + 1, &set, NULL, NULL, NULL);

        int clientfd = accept(sockfd, (struct sockaddr *)NULL, NULL);

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

            int address = 0;

            printf("Send Address to client\n");

            write(clientfd, &address, sizeof(address));

            dup2(fds[1], 1);
            closeAll();

            while (1)
            {
                select(clientfd + 1, &clientSet, NULL, NULL, NULL);

                if (read(clientfd, buffer, sizeof(buffer)) > 0)
                {
                    printf("%s", buffer);
                }
                // sleep(INTERVAL);
            }

            exit(0);
            // child
        }
        else
        {
            dup2(fds[0], 0);
            closeAll();
            while (1)
            {
                read(0, &buffer, sizeof(buffer));
                printf("Server Main: %s", buffer);
                // sleep(INTERVAL);
            }

            // parent
        }
    }
}