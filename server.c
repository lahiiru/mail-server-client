#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <string.h>
#include <dirent.h>
#include <errno.h>
#include <arpa/inet.h>
#include <stddef.h>

#define BUFFER_SIZE 5000
#define on_error(...) { fprintf(stderr, __VA_ARGS__); fflush(stderr); exit(1); }
#define on_info(...) { fprintf(stdout, __VA_ARGS__); fflush(stdout); }

int server_fd;
struct sockaddr_in client;
// Declaration of thread condition variable 
pthread_cond_t pool_full = PTHREAD_COND_INITIALIZER; 
pthread_mutex_t pool_full_mutex = PTHREAD_MUTEX_INITIALIZER;
int additional_thread_needed = 0;
// declaring mutex
pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t client_list_lock = PTHREAD_MUTEX_INITIALIZER;
int active_sessions = 0;
int thread_count = 0;
int pool_usage = 0;
int pool_size=1;
int free_threads=0;
char* welcomMessage = "\n ~ WELCOME ~ \n\n";

char * get_ip(int sockfd) {

    struct sockaddr_in addr;
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int res = getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
    char *clientip = malloc(100);
    strcpy(clientip, inet_ntoa(addr.sin_addr));
	return clientip;

}

char * get_port(int sockfd) {
    struct sockaddr_in addr;
	char *port = malloc(100);
    socklen_t addr_size = sizeof(struct sockaddr_in);
    int res = getpeername(sockfd, (struct sockaddr *)&addr, &addr_size);
	sprintf(port,"%ld", ntohs(addr.sin_port));
	return port; 
}

int startsWith(const char *pre, const char *str)
{
    size_t lenpre = strlen(pre),
           lenstr = strlen(str);
    return lenstr < lenpre ? 0 : strncmp(pre, str, lenpre) == 0;
}

void put_in_file(char * data, char * file_name) {
	FILE *fp;
	fp = fopen(file_name, "w"); 
	if (fp != NULL) {
		fputs(data, fp);
		fclose(fp);
	} else {
		on_info("Invalid file path");
	}
}

char * get_value_from_file(char * file_name) {
	char *data = malloc(1000);
	FILE *fp = fopen(file_name, "r");
	if (fp != NULL) {
		size_t newLen = fread(data, sizeof(char), 1000, fp);
		if (newLen == 0) {
			fputs("Error reading file", stderr);
		} else {
			data[newLen] = '\0'; /* Just to be safe. */
		}
		fclose(fp);
	} else {
		data[0] = '0';
		data[1] = '\0';
	}
	return data;
}

char * build_mail_box_info(DIR* fd, char * cname) {
	FILE *entry_file;
	char buffer[1000];
	struct dirent* in_file;
	char* info = malloc(1000);
	memset(info,0,strlen(info));
	while ((in_file = readdir(fd))) {
			/* On linux/Unix we don't want current and parent directories
			 * On windows machine too, thanks Greg Hewgill
			 */
			if (!strcmp (in_file->d_name, "."))
				continue;
			if (!strcmp (in_file->d_name, ".."))    
				continue;
			if (!strcmp (in_file->d_name, "id"))    
				continue;
			/* Open directory entry file for common operation */
			char* file_path = malloc(1000);
			strcpy(file_path, "./");
			strcat(file_path, cname);
			strcat(file_path, "/");
			strcat(file_path, in_file->d_name);

			entry_file = fopen(file_path, "r");
			if (entry_file == NULL)
			{
				fprintf(stderr, "Error : Failed to open entry file - %s\n", strerror(errno));
				return 1;
			}
			char *token = strtok(in_file->d_name, "_"); // "file_" removes 
			char *mail_id = strtok(NULL, "");
			strcat(info, "Id: ");
			strcat(info, mail_id);
			strcat(info, "\n");
			while (fgets(buffer, 1000, entry_file) != NULL)
			{
				if (!strcmp (buffer, "\n")) break;
				strcat(info, buffer);
			}
			strcat(info, "\n");
			/* When you finish with the file, close it */
			fclose(entry_file);
	}
	return info;
}

char* parseMessage(char *buf, int len, char * ip, char * port) {
	char *response = malloc(BUFFER_SIZE+1);
	char *message = malloc(BUFFER_SIZE+1);
	memset(message,0,strlen(message));
	memset(response,0,strlen(response));
	if (buf[len-1]=='\r') len--; // remove tralling CR
	if (buf[len-1]=='\n') len--; // remove tralling LF
	memcpy(message, buf, len);
	message[len] = '\0';
	
	if (startsWith("test", message)) {
		memcpy(response, buf, len);
	} else if (startsWith("make ", message)) {
		char *token = strtok(message, " "); // "make " removes 
		char *name = strtok(NULL, "");
		
		DIR* dir = opendir(name);
		if (dir)
		{
			/* Directory exists. */
			closedir(dir);
			return "ALREADY EXIST\n";
		} else if (ENOENT == errno) {
			/* Directory does not exist. */
			mkdir(name , 0777);
			time_t timex;
			time(&timex);
			
			pthread_mutex_lock(&client_list_lock);
			char buffer[200]; 
			FILE *file;

			file = fopen("client_list.txt", "a"); 

			fputs(name, file);fputs(",0,", file);
			fputs(ip, file);fputs(",", file);
			fputs(port, file);fputs(",", file);
			fputs(ctime(&timex), file);
			fclose(file);
			pthread_mutex_unlock(&client_list_lock);

		} else {
			/* opendir() failed for some other reason. */
			return "FAILD\n";
		}
		return "DONE\n";
	} else if (startsWith("get_client_list", message)) {
		char *data = malloc(1000);
		FILE *fp = fopen("client_list.txt", "r");
		if (fp != NULL) {
			size_t newLen = fread(data, sizeof(char), 1000, fp);
			if (newLen == 0) {
				fputs("Error reading file", stderr);
			} else {
				data[newLen] = '\0'; /* Just to be safe. */
			}
			fclose(fp);
		}
		return data;
	} else if (startsWith("send ", message)) {
		char *token = strtok(message, " "); // "send " removes 
		char *rname = strtok(NULL, " ");
		char *actual_body = strtok(NULL, "");
		if (actual_body == NULL || strlen(actual_body) == 1) return "EMPTY MESSAGE\n";
		
		char *body = malloc(1000);
		strcpy(body, "Status: unread\n");
		strcat(body, actual_body);
		DIR* dir = opendir(rname);
		if (dir)
		{
			/* Directory exists. */
			closedir(dir);
			char* file_path = malloc(1000);
			char* mail_path = malloc(1000);
			strcpy(file_path, "./");
			strcat(file_path, rname);
			strcat(file_path, "/");
			strcpy(mail_path, file_path);
			strcat(file_path, "id");
			char * str_value = get_value_from_file(file_path);
			int next_value = atoi(str_value) + 1;
			char* str_next_value = malloc(100);
			sprintf(str_next_value, "%d", next_value);
			put_in_file(str_next_value, file_path);
			strcat(mail_path, "file_");
			strcat(mail_path, str_next_value);
			put_in_file(body, mail_path);
			return "DONE\n";
		} else if (ENOENT == errno) {
			/* Directory does not exist. */
			return "INVALID RECIPIENT\n";
		} else {
			/* opendir() failed for some other reason. */
			return "FAILD\n";
		}
		
	} else if (startsWith("get_mailbox ", message)) {
		char *token = strtok(message, " "); // "get_mailbox " removes 
		char *cname = strtok(NULL, "");
		DIR* dir = opendir(cname);
		if (dir)
		{
			/* Directory exists. */
			char * info = build_mail_box_info(dir, cname);
			closedir(dir);
			return info;
		} else {
			return "INVALID USER\n";
		}
	} else if (startsWith("read ", message)) {
		char *token = strtok(message, " "); // "read " removes 
		char *cname = strtok(NULL, " ");
		char *m_id = strtok(NULL, "");
		char *mail_path = malloc(1000);
		memset(mail_path,0,strlen(mail_path));
		char buffer[1000];
		
		strcpy(mail_path, "./");
		strcat(mail_path, cname);
		strcat(mail_path, "/file_");
		strcat(mail_path, m_id);

		char *data = malloc(1000);
		char *readed_mail = malloc(1000);
		memset(data,0,strlen(data));
		memset(readed_mail,0,strlen(readed_mail));
		FILE *fp = fopen(mail_path, "r");
		if (fp == NULL)
		{
			on_info(mail_path);
			return "INVALID MAIL ID OR USER\n";
		}

		while (fgets(buffer, 1000, fp) != NULL)
		{
			if (!strncmp (buffer, "Status:", 7)) {
				strcat(readed_mail, "Status: read\n");
			} else {
				strcat(data, buffer);
				strcat(readed_mail, buffer);
			}
		}

		/* When you finish with the file, close it */
		fclose(fp);
		put_in_file(readed_mail, mail_path);
		return data;
	} else if (startsWith("delete ", message)) {
		char *token = strtok(message, " "); // "read " removes 
		char *cname = strtok(NULL, " ");
		char *m_id = strtok(NULL, "");
		char *mail_path = malloc(1000);
		memset(mail_path,0,strlen(mail_path));
		
		strcpy(mail_path, "./");
		strcat(mail_path, cname);
		strcat(mail_path, "/file_");
		strcat(mail_path, m_id);
		
		if (remove(mail_path) == 0) {
			return "DONE\n";
		}
		return "INVALID MAIL ID OR USER\n";
	} else {
		return "INVALID REQUEST\n";
	}
	free(message);
	return response;
}
/* this function is run by the second thread */
void *request_handler(void *args)
{
	int client_fd;
	char buf[BUFFER_SIZE];
	int err;
	int thread_in_pool = (int)args;
	pthread_mutex_lock(&mutex);
	thread_count++;
	pthread_mutex_unlock(&mutex);
	do {
		pthread_mutex_lock(&mutex);
		free_threads++;
		on_info("free threads %i/%i\n",free_threads,thread_count);
		pthread_mutex_unlock(&mutex);
		
		socklen_t client_len = sizeof(client);
		client_fd = accept(server_fd, (struct sockaddr *) &client, &client_len);
		send(client_fd, welcomMessage, strlen(welcomMessage), 0);
		if (client_fd < 0) on_error("Could not establish new connection\n");
		
		pthread_mutex_lock(&mutex);
		active_sessions++;
		free_threads--;
		on_info("free threads %i/%i\n",free_threads,thread_count);
		if (thread_in_pool) pool_usage++;
		pthread_mutex_unlock(&mutex);
		
		if (free_threads == 0) {
			pthread_mutex_lock(&pool_full_mutex);
			additional_thread_needed = 1;
			pthread_cond_signal(&pool_full);
			pthread_mutex_unlock(&pool_full_mutex);
		}
		
		on_info("sessions %i, pool thread usage %i/%i\n",active_sessions,pool_usage,pool_size);
		
		while (1) {
			int read = recv(client_fd, buf, BUFFER_SIZE, 0);

			if (!read) break; // done reading
			if (read < 0) on_error("Client read failed\n");
			buf[read+1]='\0';

			if (strncmp(buf, "quit", 4) == 0) {
				err = send(client_fd, "bye!\n", 5, 0);
				close(client_fd);
				break;
			}

			char* response = parseMessage(buf, read, get_ip(client_fd), get_port(client_fd));
			err = send(client_fd, response, strlen(response), 0);
			if (err < 0) on_error("Client write failed\n");
		}
		pthread_mutex_lock(&mutex);
		active_sessions--;
		if (thread_in_pool) pool_usage--;
		pthread_mutex_unlock(&mutex);
		
		on_info("sessions %i, pool thread usage %i/%i\n",active_sessions,pool_usage,pool_size);
	} while (thread_in_pool);

	pthread_mutex_lock(&mutex);
	thread_count--;
	on_info("free threads %i/%i\n",free_threads,thread_count);
	pthread_mutex_unlock(&mutex);
	on_info("Thread exit\n");

	return NULL;
}

int main(int argc, char* argv[])
{
	if (argc < 3) on_error("Usage: %s [threads] [port]\n", argv[0]);

	int port = atoi(argv[2]);
	pool_size = atoi(argv[1]);

	int err;
	struct sockaddr_in server;

	server_fd = socket(AF_INET, SOCK_STREAM, 0);
	if (server_fd < 0) on_error("Could not create socket\n");

	server.sin_family = AF_INET;
	server.sin_port = htons(port);
	server.sin_addr.s_addr = INADDR_ANY;

	int opt_val = 1;
	setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof opt_val);

	err = bind(server_fd, (struct sockaddr *) &server, sizeof(server));
	if (err < 0) on_error("Could not bind socket\n");

	err = listen(server_fd, 128);
	if (err < 0) on_error("Could not listen on socket\n");

	printf("Server is listening on %d\n", port);

	/* this variable is our reference to the second thread */
	pthread_t threads[pool_size];
	int pool = 1;
	int i=0;
	for(;i<pool_size;i++) {
		/* create a second thread which executes inc_x(&x) */
		if(pthread_create(&threads[i], NULL, request_handler, (void *)pool)) {
			fprintf(stderr, "Error creating thread\n");
			return 1;
		}
	}
	pool = 0;
	while (1) {
		pthread_mutex_lock(&pool_full_mutex);
		pthread_t t;
		printf("main thread sleeping...\n");
		while (!additional_thread_needed)
			pthread_cond_wait(&pool_full, &pool_full_mutex);
		printf("additional thread needed!\n");		
		/* create a second thread which executes inc_x(&x) */
		if(pthread_create(&t, NULL, request_handler, (void *)pool)) {
			fprintf(stderr, "Error creating thread\n");
			return 1;
		}
		additional_thread_needed = 0;
		pthread_mutex_unlock(&pool_full_mutex);
	}
	int j=0;
	for(;i<pool_size;i++) {
		/* wait for the second thread to finish */
		if(pthread_join(threads[i], NULL)) {
			fprintf(stderr, "Error joining thread\n");
			return 2;
		}
	}

	printf("main thread exit\n");

	return 0;
}

