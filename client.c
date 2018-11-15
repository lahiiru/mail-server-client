/*
    C ECHO client example using sockets
*/
#include<stdio.h> //printf
#include<string.h> //strcpy
#include<sys/socket.h>
#include<netdb.h> //hostent
#include<arpa/inet.h>
#include <pthread.h>

#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }
#define on_info(...) { fprintf(stdout, __VA_ARGS__); fflush(stdout); }

int sock;

char* getIP(char* hostname) {
	char *ip = malloc(100);
    struct hostent *he;
    struct in_addr **addr_list;
    int i;
         
    if ( (he = gethostbyname( hostname ) ) == NULL) 
    {
        //gethostbyname failed
        herror("gethostbyname");
        return 1;
    }
     
    //Cast the h_addr_list to in_addr , since h_addr_list also has the ip address in long format only
    addr_list = (struct in_addr **) he->h_addr_list;
     
    for(i = 0; addr_list[i] != NULL; i++) 
    {
        //Return the first one;
        strcpy(ip , inet_ntoa(*addr_list[i]) );
    }
    return ip;
}

int endsWith(char *str, char* end)
{
    if (strlen(end) > strlen(str))
        return 0;
    if (strcmp(&str[strlen(str)-strlen(end)],end) == 0)
        return 1;
    return 0;
}

int startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}

void * receiver(void *args) {
	char *server_reply = malloc(5000);
	
	while (1) {
		memset(server_reply,0,5000);
		if( recv(sock , server_reply , 2000 , 0) < 0) {
			on_error("recv failed");
			break;
		}
		if (strlen(server_reply) > 0) {
			on_info(server_reply);
			if (strncmp(server_reply, "bye", 3) == 0) {
				break;
			}
			on_info("\n> ");
		}
	}
	
	free(server_reply);
	
	return NULL;
}

int main(int argc , char *argv[])
{
	if (argc < 3) on_error("Usage: %s [ip] [port]\n", argv[0]);

	int port = atoi(argv[2]);
	char* ip = getIP(argv[1]);
    struct sockaddr_in server;

	char *message = malloc(5000);
     
    //Create socket
    sock = socket(AF_INET , SOCK_STREAM , 0);
    if (sock == -1)
    {
        on_error("Could not create socket");
    }
     
    server.sin_addr.s_addr = inet_addr(ip);
    server.sin_family = AF_INET;
    server.sin_port = htons( port );
 
    //Connect to remote server
    if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
    {
        on_error("connect failed. Error");
        return 1;
    }
     
    on_info("Connected\n");
	pthread_t t;
	int data = 0;
	int ch;
	char* mail = malloc(5000);
	int writing_mail = 0;
	int len;
	
	if(pthread_create(&t, NULL, receiver, (void *)data)) {
		on_error("Error creating receiver thread\n");
		return 1;
	}
    //keep communicating with server
    while(1)
    {	
		memset(message,0,5000);
		len = 0;

		while(EOF!=scanf("%c",&ch) && ch != '\n'){
			message[len++]=ch;
		}
		message[len++]='\0';

        if(writing_mail == 0 && startsWith("send ", message)) { // need to read message for mail from client
			writing_mail = 1;
			memset(mail,0,strlen(mail));
			strcpy(mail, message);
			on_info("From: ");
			fgets(message, 200, stdin);
			strcat(mail, " From: ");
			strcat(mail, message);
			on_info("To: ");
			fgets(message, 200, stdin);
			strcat(mail, "To: ");
			strcat(mail, message);
			strcat(mail, "Date: ");
			time_t timex;
			time(&timex);
			strcat(mail, ctime(&timex));
			strcat(mail, "\n"); // header end
			on_info("Enter your message (Press <Enter><Enter>.<Enter><Enter> to end the message):\n");
			memset(message,0,5000);
			len = 0;
			while(EOF!=scanf("%c",&ch)) {
				int l = strlen(mail);
				mail[l] = ch;
				mail[l+1] = '\0';
				if(endsWith(mail, "\n\n.\n\n") || endsWith(mail, "\r\n.\r\n")) {
					writing_mail = 0;
					on_info("END\n");
					strcpy(message, mail);
					l = strlen(message);
					message[l - 4] = '\0';
					break;
				}
			}
		}
        if (writing_mail == 0) {
			if( send(sock , message , strlen(message) , 0) < 0)
			{
				on_info("send failed.");
				return 1;
			}
		}

		if (startsWith("quit", message)) {
			break;
		}
    }
	if(pthread_join(t, NULL)) {
		on_error("Error joining thread\n");
		return 2;
	}
    close(sock);
    return 0;
}