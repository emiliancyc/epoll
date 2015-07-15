#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/epoll.h>
#include <arpa/inet.h>
#include "user_list.h"

//extern int errno;

int srv_fd = -1;
int epoll_fd = -1;

size_t receive_msg(char** msg, int fd);
int accept_new_client();
void serve_client(int fd, uint32_t events, user_list*);
void handle_login(const char* msg, size_t len, int fd, user_list*);
void handle_user_list(const char* msg, size_t len, int fd, user_list*);

int main(int argc, const char *argv[]) {

	if (argc != 2) {
		printf("Zla liczba argumentow! Podaj tylko jeden!\n");
		return -1;
	}

	time_t t;
	srand((unsigned) time(&t));
	unsigned short int port = ((rand() % 500) + 8000);
	printf("%d\n", port);

	int CNT = atoi(argv[1]);

	struct sockaddr_in srv_addr;
	struct epoll_event e, es[CNT]; //e dany event  es[] tablica eventow do obslugi epoll-wielu klientow
	memset(&srv_addr, 0, sizeof(srv_addr)); // memset zeruje nam te strukturki
	memset(&e, 0, sizeof(e));

	srv_fd = socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK, 0);
	if (srv_fd < 0) {
		printf("Cannot create socket\n");
		return 1;
	}

	srv_addr.sin_family = AF_INET;
	srv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	srv_addr.sin_port = htons(port);

	if (bind(srv_fd, (struct sockaddr*) &srv_addr, sizeof(srv_addr)) < 0) {
		printf("Cannot bind socket\n");
		close(srv_fd);
		return 1;
	}

	if (listen(srv_fd, 1) < 0) {
		printf("Cannot listen\n");
		close(srv_fd);
		return 1;
	}

	epoll_fd = epoll_create(2);
	if (epoll_fd < 0) {
		printf("Cannot create epoll\n");
		close(srv_fd);
		return 1;
	}

	e.events = EPOLLIN;
	e.data.fd = srv_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, srv_fd, &e) < 0) {
		printf("Cannot add server socket to epoll\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}

	size_t cap = CNT;
	user_list* lista = create_ul(cap);

	int i = 0;
	for (;;) {
		i = epoll_wait(epoll_fd, es, CNT, -1);

		if (i < 0) {
			printf("Cannot wait for events\n");
			close(epoll_fd);
			close(srv_fd);
			return 1;
		}

		for (--i; i > -1; --i) {
			if (es[i].data.fd == srv_fd) { //pobiera nam deskryptor i jesli to bedzie deskryptor serwera, to wchodzi w if'a
				if (accept_new_client() != 0)
					return 1; //shut down server
			} else {
				serve_client(es[i].data.fd, es[i].events, lista);
			}
		}
	}

	return 0;
}

int accept_new_client() {
	int cli_fd = -1;
	struct sockaddr_in cli_addr;
	socklen_t cli_addr_len = sizeof(cli_addr);
	struct epoll_event e;
	memset(&cli_addr, 0, sizeof(cli_addr));

	cli_fd = accept(srv_fd, (struct sockaddr*) &cli_addr, &cli_addr_len); // i akceptuje

	if (cli_fd < 0) { //ustawia tam -1 dla klienta dla porzadku
		printf("Cannot accept client\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}

	e.events = EPOLLIN;
	e.data.fd = cli_fd;
	if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, cli_fd, &e) < 0) {
		printf("Cannot accept client\n");
		close(epoll_fd);
		close(srv_fd);
		return 1;
	}

	return 0;
}

void serve_client(int fd, uint32_t events, user_list* lista) {
	char *msg = 0;
	size_t len = 0;

	if (events & EPOLLIN) {
		len = receive_msg(&msg, fd);
		if (len > 0) {
			if (msg[0] == '2') {
				printf("%s\n", msg);
				handle_login(msg, len, fd, lista);
			} else if (msg[0] == '6') {
				printf("%s\n", msg);
				handle_user_list(msg, len, fd, lista);
			}
		}
		//free(msg);
	} else {
		close(fd);
		epoll_ctl(epoll_fd, EPOLL_CTL_DEL, fd, 0);
		//TODO remove from user list
	}
}

size_t receive_msg(char** msg, int fd) {
	size_t len = 0;
	if (read(fd, &len, sizeof(size_t)) != sizeof(size_t)) {
		return 0;
	}

	(*msg) = malloc((len + 1) * sizeof(char));
	(*msg)[len] = 0;

	if (read(fd, (*msg), len) != len) {
		free(*msg);
		(*msg) = 0;
		return 0;
	}

	return len;
}

void handle_login(const char* msg, size_t len, int fd, user_list* lista) {
	//TODO:
	//		* create user
	//		* add user to user list
	//		* send ack ("1.0") if user added, send nack otherwise ("1.1.errromessage")
	//		* if nack sent then clear user (close its fd, remove from epoll and remove from user list if needed)

	size_t size = strlen(msg); //długość wiadomości
	char* temp = malloc(size * sizeof(char));
	strncpy(temp, msg + 2, size - 2); //przepisanie od 2 miejsca w stringu
	//printf("%d\n", size);
	//printf("%s\n", temp);
	temp[size] = 0;
	user *ptr = malloc(sizeof(user));
	ptr->fd = fd; //wskaznik na strukture user
	ptr->nick = temp;

	if (!(lista->add(lista, ptr))) { //dodaje do listy
		char* Ack = "1.0";
		len = strlen(Ack) + 1;
		write(fd, &len, sizeof(size_t));
		write(fd, Ack, len);
	} else {
		char* NAck = "1.1.Blad logowania";
		len = strlen(NAck) + 1;
		write(fd, &len, sizeof(size_t));
		write(fd, NAck, len);
	}

}

void handle_user_list(const char* msg, size_t len, int fd, user_list* lista) {

	len = 0;
	char** result = lista->print_users(lista, &len);
	int i = 0;
	//	printf("%s\n", result[i]);
	//}

//	len=strlen(usr_list);
	//write(fd, &len, sizeof(size_t));
	//write(fd, usr_list, len);

}
