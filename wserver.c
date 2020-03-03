#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <signal.h>
#include <arpa/inet.h>
#include "config.h"


#include <stdio.h>
#include <pthread.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include "request.h"
#include "io_helper.h"
#include "common_threads.h"



char default_root[] = ".";
int use_ptr  = 0;
int fill_ptr = 0;
int num_full = 0;
buffer_ele* buffer; // buffer to hold file descriptors
pthread_cond_t empty = PTHREAD_COND_INITIALIZER;
pthread_cond_t fill = PTHREAD_COND_INITIALIZER;
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;

void do_fill(buffer_ele peer_info_ptr, int buffers) {
    buffer[fill_ptr] = peer_info_ptr;
    fill_ptr = (fill_ptr + 1) % buffers;
    num_full++;
}


void producer(int listen_fd, int buffers) {
	printf("server: waiting for connections...\n");

	while (1) { // master thread accept() loop
		struct sockaddr_in client_addr;
		int client_len = sizeof(client_addr);
		int conn_fd = accept_or_die(listen_fd,
									(sockaddr_t *) &client_addr,
									(socklen_t *) &client_len);

		buffer_ele peer_info_ptr = malloc(sizeof(struct peer_info));

		peer_info_ptr->fd = conn_fd;
		inet_ntop(client_addr.sin_family,
					&(client_addr.sin_addr),
					peer_info_ptr->ip_str,
					sizeof peer_info_ptr->ip_str);
		
		peer_info_ptr->port = ntohs(client_addr.sin_port);
        
		Mutex_lock(&mutex);
	    while (num_full == buffers)
	        Cond_wait(&empty, &mutex);
	    do_fill(peer_info_ptr, buffers);
	    Cond_signal(&fill);
	    Mutex_unlock(&mutex);
	}
}

buffer_ele do_get(int buffers) {
    buffer_ele tmp = buffer[use_ptr];
    use_ptr = (use_ptr + 1) % buffers;
    num_full--;
    return tmp;
}

void *consumer(void *arg) {
	int buffers = *((int *) arg);
    // consumer: keep pulling data out of shared buffer
    while (1) {
		Mutex_lock(&mutex);
		while (num_full == 0)
	    	Cond_wait(&fill, &mutex);
		buffer_ele peer_info_ptr = do_get(buffers);
		Cond_signal(&empty);
		Mutex_unlock(&mutex);
		request_handle(peer_info_ptr);
		close_or_die(peer_info_ptr->fd);
		free(peer_info_ptr);
    }
	
    return NULL;
}

//
// ./wserver [-d <basedir>] [-p <portnum>]
//
int main(int argc, char *argv[]) {
    int c;
    char *root_dir = ROOTDIR;
    int port = PORT;            // default port
	int threads = NUMOFTHREADS; // default number of workers
	int buffers = BUFFERSIZE;   // default buffer size
	
	buffer = (buffer_ele *) malloc(buffers * sizeof(buffer_ele));
    assert(buffer != NULL);

    
    while ((c = getopt(argc, argv, "d:p:")) != -1) {
		switch (c) {
			case 'd':
	    		root_dir = optarg;
	    		break;
			case 'p':
	    		port = atoi(optarg);
	    		break;
			default:
	    		fprintf(stderr, "usage: wserver [-d basedir] [-p port]\n");
	    		exit(1);
		}
	}

    // run out of this directory
    chdir_or_die(root_dir);

	// create the threads pool
	pthread_t cid[threads];
	int i;
    for (i = 0; i < threads; i++) {
	    Pthread_create(&cid[i], NULL, consumer, &buffers);
    }

    // get to work
    int listen_fd = open_listen_fd_or_die(port);
    producer(listen_fd, buffers);
	

	free(buffer);
    return 0;
}



    


 
