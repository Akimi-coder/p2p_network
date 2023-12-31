extern "C" {
#include <sys/epoll.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>
}

            #include <iostream>
#include <thread>

#define MAX 1024


// initialize for epoll
int ret;
int fd, cfd;
struct sockaddr_in ssin;
struct sockaddr_in client_sin;
socklen_t socklen = sizeof(struct sockaddr);
int epfd;
struct epoll_event ev;
struct epoll_event evlist[MAX];

int nfds;

void sigIntHandler(int signum)
{
    std::cout << "Exit by interrupt signal" << std::endl;
    for (int i = 0; i < nfds; i++)
    {
        close(evlist[i].data.fd);
    }
    exit(0);
}


int main(int argc, char *argv[])
{
    signal(SIGINT, sigIntHandler);

    memset(&ssin, 0, socklen);
    ssin.sin_family      = AF_INET;
    ssin.sin_port    = htons(atoi(argv[1]));
    ssin.sin_addr.s_addr = htonl(INADDR_ANY);

    // socket, bind, listen
    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (0 > fd) { perror("socket"); exit(1); }
    if (0 > bind(fd, (struct sockaddr *)&ssin, socklen)) { perror("bind"); exit(1); }
    if (0 > listen(fd, 5)) { perror("listen"); exit(1); }


    // epoll_create1
    epfd = epoll_create1(0);
    if (0 > epfd) { perror("epoll_create1"); exit(1); }
    // epoll_ctl
    ev.events = EPOLLIN;
    ev.data.fd = STDIN_FILENO; // for quit
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, STDIN_FILENO, &ev);
    if (0 > ret) { perror("epoll_ctl"); exit(1); }
    ev.data.fd = fd;
    ret = epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ev);
    if (0 > ret) { perror("epoll_ctl"); exit(1); }

    int i;
    char buf[BUFSIZ];
    int buflen;

    for (;;)
    {
        // epoll
        nfds = epoll_wait(epfd, evlist, MAX, -1);
        if (ret == -1) { perror("epoll_wait"); exit(1); }
        std::cout << nfds;

        for (i = 0; i < nfds; i++)
        {
            if (evlist[i].data.fd == STDIN_FILENO)
            {
                fgets(buf, BUFSIZ-1, stdin);
                if (!strcmp(buf, "quit") || !strcmp(buf, "exit"))
                {
                    close(fd);
                    exit(0);
                }
            }
            else if (evlist[i].data.fd == fd)
            {
                // accept
                cfd = accept(fd, (struct sockaddr *)&client_sin, &socklen);
                if (cfd == -1) { perror("accept"); exit(1); }
                printf("\x1b[0;32m[*] accept\x1b[0m\n");
                // epoll_ctl
                ev.events = EPOLLIN;
                ev.data.fd = cfd;
                ret = epoll_ctl(epfd, EPOLL_CTL_ADD, cfd, &ev);
                if (ret == -1) { perror("epoll_ctl"); exit(1); }
            }
            else
            {
                cfd = evlist[i].data.fd;
                // read
                buflen = read(cfd, buf, BUFSIZ-1);
                if (buflen == 0)
                {
                    // close
                    close(cfd);
                    if (cfd == -1) { perror("close"); exit(1); }
                    printf("\x1b[0;31m[*] close\x1b[0m\n");
                    // epoll_ctl
                    epoll_ctl(epfd, EPOLL_CTL_DEL, cfd, &evlist[i]);
                    if (ret == -1) { perror("epoll_ctl"); exit(1); }
                }
                else
                {
                    // do something with data
                    buf[buflen] = '\0';
                    std::string msgPrefix = "some-prefix-to-prevent-arbitrary-connection";
                    std::string msg = buf;

                    if (msgPrefix.length() > msg.length()) continue;
                    if (!strcmp(msgPrefix.c_str(), msg.substr(0, msgPrefix.length()).c_str()))
                    {
                        msg = msg.substr(msgPrefix.length(), msg.length());
                        std::cout << msg << std::endl;

                        //write(cfd, buf, buflen);  // write back to client if needed
                    } // else, reject
                }
            }
        }
    }

    return 0;
}
