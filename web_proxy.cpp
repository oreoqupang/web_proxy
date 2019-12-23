#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <iostream>
#include <map>
#include <string>
#include <algorithm>

#define TCP_MAX 65535
using namespace std;

struct info{
    int childfd;
    int port_num;
};

struct info2{
    string host_name;
    int client_fd;
    int server_fd;
};


int broad_cast_mode = 0;
map<string, int> sessions;
pthread_mutex_t lock;

void * session2(void * arg){
    struct info2 fd_info = *(struct info2 *)(arg);
    int client_fd = fd_info.client_fd;
    int server_fd = fd_info.server_fd;
    char buf[TCP_MAX];

    while(true){
        ssize_t received = recv(server_fd, buf, TCP_MAX, 0);

        if(received == 0 || received == -1) {
			break;
		}
        write(1, buf, received);
        ssize_t sent = send(client_fd, buf, received, 0);
		if (sent == 0) {
			break;
		}
	}

    pthread_mutex_lock(&lock);
    sessions.erase(fd_info.host_name);
    close(client_fd);
    close(server_fd);
    pthread_mutex_unlock(&lock);
    return NULL;
}

void * session(void * arg){
	struct info tmp_info = *(struct info *)(arg);
    int client_fd = tmp_info.childfd, server_fd;
    string host_name;
	   char buf[TCP_MAX];

    ssize_t received = recv(client_fd, buf, TCP_MAX, 0);
    if(received == 0 ||received == -1) {
		perror("recv failed");
		return NULL;
    }

    if(!memcmp(buf, "GET", 3) || !memcmp(buf, "POST", 4) || !memcmp(buf, "HEAD", 4) || !memcmp(buf, "PUT", 3) || !memcmp(buf, "DELETE", 6) || !memcmp(buf, "OPTIONS", 7)){
        char * host_ptr = ((char *)(strstr(buf, "Host:"))+6);
        host_name = "";
        for(; *host_ptr != 0xd; host_ptr++){
            host_name += *host_ptr;
        }
        cout<<"host_name :"<<host_name<<endl;

        if(sessions.find(host_name) == sessions.end()){
            pthread_mutex_lock(&lock);
            sessions.insert(make_pair(host_name, tmp_info.port_num));
            pthread_mutex_unlock(&lock);

            int server_fd = socket(AF_INET, SOCK_STREAM, 0);
	        if (server_fd == -1) {
		        perror("socket failed");
		        return NULL;
	        }

            struct addrinfo hints;
            struct addrinfo *servinfo;
            struct sockaddr_in addr;
            memset(&hints, 0, sizeof(hints));
            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;

            getaddrinfo(host_name.c_str(), "80", &hints, &servinfo);
          	addr.sin_family = AF_INET;
          	addr.sin_port = htons(80);
          	memcpy(&(addr.sin_addr), &(((struct sockaddr_in *)(servinfo->ai_addr))->sin_addr), sizeof(struct in_addr));
	          memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	          int res = connect(server_fd,reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
            printf("sssserver fd ; %d\n", server_fd);
	           if (res == -1) {
		             perror("connect failed");
		             return NULL;
	        }
	        printf("web server connected\n");
          write(1, buf, received);

          ssize_t sent = send(server_fd, buf, received, 0);
			    if (sent == 0) {
            perror("First HTTP request");
				    return NULL;
			    }
            pthread_t tid;
            struct info2 info2;
            info2.client_fd = client_fd;
            info2.server_fd = server_fd;
            info2.host_name = host_name;
            if(pthread_create(&tid, NULL, session2, (void *)&info2)){
                perror("session2 thread create");
			            return NULL;
            }

            while(true){
                received = recv(client_fd, buf, TCP_MAX, 0);
                if(received == 0 || received == -1) {
			        break;
                }
                write(1, buf, received);
			    sent = send(server_fd, buf, received, 0);
			    if (sent == 0) {
				    break;
			    }
		    }
      }
    }// if http request

    pthread_mutex_lock(&lock);
    close(client_fd);
    close(server_fd);
    sessions.erase(host_name);
    pthread_mutex_unlock(&lock);

    return NULL;
}


int main(int argc, char * argv[]) {
	if(argc != 2){
		printf("syntax : web_proxy <port>\n");
		return -1;
	}

	if (pthread_mutex_init(&lock, NULL) != 0) {
        	perror("mutex init failed");
        	return -1;
	}

	int sockfd = socket(AF_INET, SOCK_STREAM, 0);
	if (sockfd == -1) {
		perror("socket failed");
		return -1;
	}

	int optval = 1;
	setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR,  &optval , sizeof(int));

	struct sockaddr_in addr;
	addr.sin_family = AF_INET;
	addr.sin_port = htons(atoi(argv[1]));
	addr.sin_addr.s_addr = htonl(INADDR_ANY);
	memset(addr.sin_zero, 0, sizeof(addr.sin_zero));

	int res = bind(sockfd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(struct sockaddr));
	if (res == -1) {
		perror("bind failed");
		return -1;
	}

	res = listen(sockfd, 5);
	if (res == -1) {
		perror("listen failed");
		return -1;
	}

	while (true) {
		pthread_t tmp;
		struct sockaddr_in addr;
		socklen_t clientlen = sizeof(sockaddr);
        struct info tmp_info;
		int childfd = accept(sockfd, reinterpret_cast<struct sockaddr*>(&addr), &clientlen);
		if (childfd < 0) {
			perror("ERROR on accept");
			break;
		}
		printf("connected\n");
		tmp_info.childfd = childfd;
        tmp_info.port_num = ntohs(addr.sin_port);
		if(pthread_create(&tmp, NULL, session, (void *)&tmp_info)){
			perror("session thread create");
			return -1;
		}
	}

	close(sockfd);
	pthread_mutex_destroy(&lock);
}
