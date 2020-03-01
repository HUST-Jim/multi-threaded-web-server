#ifndef __REQUEST_H__
typedef struct peer_info {
	int fd;
	char ip_str[INET_ADDRSTRLEN];
	uint16_t port;
} * buffer_ele;

void request_handle(buffer_ele);

#endif // __REQUEST_H__
