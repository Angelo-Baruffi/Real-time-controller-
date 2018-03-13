#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>

#define BUF_SIZE 1000
#define NSEC_PER_SEC    (1000000000) /* The number of nsecs per sec. */

int prepara_socket_cliente(char *host, char *porta)
{ 
    struct addrinfo hints;
    struct addrinfo *result, *rp;
    int sfd, s;

    /* Obtain address(es) matching host/port */

    memset(&hints, 0, sizeof(struct addrinfo));
    hints.ai_family = AF_UNSPEC;    /* Allow IPv4 or IPv6 */
    hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
    hints.ai_flags = 0;
    hints.ai_protocol = 0;          /* Any protocol */

    s = getaddrinfo(host, porta, &hints, &result);
    if (s != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
        exit(EXIT_FAILURE);
    }

    /* getaddrinfo() returns a list of address structures.
       Try each address until we successfully connect(2).
       If socket(2) (or connect(2)) fails, we (close the socket
       and) try the next address. */

	for( rp = result; rp != NULL; rp = rp->ai_next) {
		sfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
        if (sfd == -1)
            continue;

        if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break;                  /* Success */

        close(sfd);
    }

    if (rp == NULL) {               /* No address succeeded */
        fprintf(stderr, "Could not connect\n");
        exit(EXIT_FAILURE);
    }

    freeaddrinfo(result);           /* No longer needed */

	return sfd;
}


int main(int argc, char* argv[])
{
        struct timespec t;
        int interval = 50000; /* 50us*/

        clock_gettime(CLOCK_MONOTONIC ,&t);
        /* start after one second */
        t.tv_sec++;

        while(1) {
                /* wait until next shot */
                clock_nanosleep(CLOCK_MONOTONIC, TIMER_ABSTIME, &t, NULL);

                /* do the stuff */

                /* calculate next shot */
                t.tv_nsec += interval;

                while (t.tv_nsec >= NSEC_PER_SEC) {
                       t.tv_nsec -= NSEC_PER_SEC;
                        t.tv_sec++;
                }
   	}
	int socket_cliente;
	int j;
   	ssize_t len;
	    ssize_t nread;
	    char buf[BUF_SIZE];

	    if (argc < 3) {
		fprintf(stderr, "Usage: %s host port msg...\n", argv[0]);
		exit(EXIT_FAILURE);
	    }

		socket_cliente = prepara_socket_cliente(argv[1], argv[2]);


	    /* Send remaining command−line arguments as separate
	       datagrams, and read responses from server */

	    for (j = 3; j < argc; j++) {
		len = strlen(argv[j]) + 1;
		        /* +1 for terminating null byte */

		if (len + 1 > BUF_SIZE) {
		    fprintf(stderr,
		            "Ignoring long message in argument %d\n", j);
		    continue;
		}


			/* Send a message */
		if (write(socket_cliente, argv[j], len) != len) {
		    fprintf(stderr, "partial/failed write\n");
		    exit(EXIT_FAILURE);
		}


			/* Receive a message */
		nread = read(socket_cliente, buf, BUF_SIZE);
		if (nread == -1) {
				perror("read");
		    exit(EXIT_FAILURE);
		}

		printf("Received %ld bytes: %s\n", (long) nread, buf);
	    }


	    exit(EXIT_SUCCESS);
}









