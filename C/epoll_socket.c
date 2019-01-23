#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/epoll.h>
#include <errno.h>

#define MAXEVENTS 64

static int
make_socket_non_blocking (int listen_sock)
{
    int flags, s;

    flags = fcntl (listen_sock, F_GETFL, 0);
    if (flags == -1)
        {
            perror ("fcntl");
            return -1;
        }

    flags |= O_NONBLOCK;
    s = fcntl (listen_sock, F_SETFL, flags);
    if (s == -1)
        {
            perror ("fcntl");
            return -1;
        }

    return 0;
}

static int
create_and_bind (char *port)
{
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int s, listen_sock;

    memset (&hints, 0, sizeof (struct addrinfo));
    hints.ai_family = AF_UNSPEC;     /* Return IPv4 and IPv6 choices */
    hints.ai_socktype = SOCK_STREAM; /* We want a TCP socket */
    hints.ai_flags = AI_PASSIVE;     /* All interfaces */

    s = getaddrinfo (NULL, port, &hints, &result);
    if (s != 0)
        {
            fprintf (stderr, "getaddrinfo: %s\n", gai_strerror (s));
            return -1;
        }

    for (rp = result; rp != NULL; rp = rp->ai_next)
        {
            listen_sock = socket (rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (listen_sock == -1)
                continue;

            s = bind (listen_sock, rp->ai_addr, rp->ai_addrlen);
            if (s == 0)
                {
                    /* We managed to bind successfully! */
                    break;
                }

            close (listen_sock);
        }

    if (rp == NULL)
        {
            fprintf (stderr, "Could not bind\n");
            return -1;
        }

    freeaddrinfo (result);

    return listen_sock;
}

int
main (int argc, char *argv[])
{
    int ret;
    struct epoll_event event;
    struct epoll_event *events;

    if (argc != 2)
        {
            fprintf (stderr, "Usage: %s [port]\n", argv[0]);
            exit (EXIT_FAILURE);
        }

    int listen_sock = create_and_bind (argv[1]);
    if (listen_sock == -1)
        abort ();

    ret = make_socket_non_blocking (listen_sock);
    if (ret == -1)
        abort ();

    ret = listen (listen_sock, SOMAXCONN);
    if (ret == -1)
        {
            perror ("listen");
            abort ();
        }

    int epollfd = epoll_create1 (0);
    if (epollfd == -1)
        {
            perror ("epoll_create");
            abort ();
        }

    event.data.fd = listen_sock;
    event.events = EPOLLIN | EPOLLET;
    ret = epoll_ctl (epollfd, EPOLL_CTL_ADD, listen_sock, &event);
    if (ret == -1)
        {
            perror ("epoll_ctl");
            abort ();
        }

    /* Buffer where events are returned */
    events = calloc (MAXEVENTS, sizeof event);

    /* The event loop */
    while (1) {
        int nfds, i;
        nfds = epoll_wait (epollfd, events, MAXEVENTS, -1);
        
        for (i = 0; i < nfds; i++) {
            if ((events[i].events & EPOLLERR) ||
                (events[i].events & EPOLLHUP) ||
                (!(events[i].events & EPOLLIN))) {
                /* fd error */
                fprintf (stderr, "epoll error\n");
                close (events[i].data.fd);
                continue;
            }
            /* fd is listen_sock */
            else if (listen_sock == events[i].data.fd) {
                /* new connection coming */
                while (1) {
                    char hbuf[NI_MAXHOST], sbuf[NI_MAXSERV];
                    struct sockaddr in_addr;                    
                    socklen_t in_len = sizeof in_addr;
                    
                    int conn_sock = accept (listen_sock, &in_addr, &in_len);
                    if (conn_sock == -1) {
                        if ((errno == EAGAIN) || (errno == EWOULDBLOCK)) {
                            /* We have processed all incoming
                               connections. */
                            break;
                        } else {
                            perror ("accept");
                            break;
                        }
                    }

                    ret = getnameinfo (&in_addr, in_len,
                                       hbuf, sizeof hbuf,
                                       sbuf, sizeof sbuf,
                                       NI_NUMERICHOST | NI_NUMERICSERV);
                    if (ret == 0) {
                        printf("Accepted connection on descriptor %d "
                               "(host=%s, port=%s)\n", conn_sock, hbuf, sbuf);
                    }

                    /* Make the incoming socket non-blocking and add it to the
                       list of fds to monitor. */
                    ret = make_socket_non_blocking (conn_sock);
                    if (ret == -1)
                        abort ();

                    event.data.fd = conn_sock;
                    event.events = EPOLLIN | EPOLLET;
                    ret = epoll_ctl (epollfd, EPOLL_CTL_ADD, conn_sock, &event);
                    if (ret == -1) {
                        perror ("epoll_ctl");
                        abort ();
                    }
                }
                continue;
            }
            else {
                /* We have data on the fd waiting to be read. Read and
                   display it. We must read whatever data is available
                   completely, as we are running in edge-triggered mode
                   and won't get a notification again for the same
                   data. */
                int done = 0;

                while (1) {
                    ssize_t count;
                    char buf[512];

                    count = read (events[i].data.fd, buf, sizeof buf);
                    if (count == -1) {
                        /* If errno == EAGAIN, that means we have read all
                           data. So go back to the main loop. */
                        if (errno != EAGAIN)
                            {
                                perror ("read");
                                done = 1;
                            }
                        break;
                    }
                    else if (count == 0) {
                        /* End of file. The remote has closed the
                           connection. */
                        done = 1;
                        break;
                    }

                    /* Write the buffer to standard output */
                    ret = write (1, buf, count);
                    if (ret == -1) {
                        perror ("write");
                        abort ();
                    }
                }

                if (done) {
                    printf ("Closed connection on descriptor %d\n",
                            events[i].data.fd);

                    /* Closing the descriptor will make epoll remove it
                       from the set of descriptors which are monitored. */
                    close (events[i].data.fd);
                }
            }
        }
    }

    free (events);

    close (listen_sock);

    return EXIT_SUCCESS;
}
