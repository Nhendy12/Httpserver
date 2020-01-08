#include <stdio.h>
#include <err.h>
#include <unistd.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <netdb.h>
#include <fcntl.h>
#include <stdint.h>
#include <string.h>
#include <iostream>
#include <math.h>
#include <ctype.h>
#include <regex.h>
#include <pthread.h>
#include <sched.h>
#include <queue>
#include "city.h"


//global variables
char* mapping_file;
int map_offsest = 1;
int map_array[8000];
int32_t cl = -1;
pthread_mutex_t lock1; //for the queue
pthread_mutex_t lock2; //for cond
pthread_mutex_t lock3; //for cond1
pthread_cond_t cond;
pthread_cond_t cond1;
std::queue<int> cl_queue;
//std::vector<int> cl_vector(1);

struct ncat{
	int valid_data;
	uint8_t *buffer;
	int FD;

	//initialize buffer
	ncat(int fd){
		FD = fd;
		buffer = (uint8_t *) malloc(4096);
		int bytesRead = read(FD, buffer, 4096);
		valid_data = bytesRead;
	}
};


//function that returns the length of a file
long filelength(const char* filpath){
	struct stat st;
	if(stat(filpath, &st))
		return -1;
	return (long) st.st_size;
}
void patchfunc(char resourse_name[28], int clp, int cLen){
	char head [55];
	char header1 [55];
	char dont_need [28];
	char existing_name [28];
	char new_name [28];
	char *buffer = (char*)malloc(1024);
	char *buffer2 = (char*)malloc(1024);
	char *doesnt_exist = (char*)"HTTP/1.1 404 File Not Found\r\nContent-Length: 0\r\n\r\n";
	char *bad_request = (char*)"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
	size_t len = 0;
	uint32_t hashreturn = 0;
	int hashresult = 0;
	resourse_name = NULL;
	//reads alias file an gets the new alias name and the name of the 
	//file the alias points to
	int x = recv(clp, buffer, cLen, 0);
	if(x == -1){
		warn("%s", "recv error");
		return;
	}
	sscanf(buffer, "%s %s %s\n", dont_need, existing_name, new_name);
	//check if existing_name is already on the server or an alias
	//if not then return 404 error not found
	if(strlen(existing_name)!= 27){
		//existing name is alias so we have to check our mapping file
		//to see if exists 400 Bad request
		len = strlen(existing_name);
		hashreturn = CityHash32(existing_name,len);
		hashresult = (hashreturn % 8000);
		//printf("%s%s%s%d\n", "hashresult is: ", existing_name, " ", hashresult);
		//if the alias does not exist in our mapping file then return bad request
		if(map_array[hashresult]==0){
			sprintf(head, bad_request, strlen(bad_request), 0);
			send(clp, head, strlen(head), 0);
			return;
		}else{
			printf("%d\n", map_array[hashresult]);
		}
	}else{
		//existing name is a file so we have to check if the resource name exists
		if(open(existing_name, O_RDWR) == -1){
			sprintf(head, doesnt_exist, strlen(bad_request), 0);
			send(clp, head, strlen(head), 0);
			return;
		}
	}
	//if alias name + existing name > 128 that is a Bad Request
	sprintf(buffer2, "%s %s\n", existing_name, new_name);
	if(strlen(buffer2)>128){
		sprintf(head, bad_request, strlen(bad_request), 0);
		send(clp, head, strlen(head), 0);
		return;
	}

	len = strlen(new_name);
	hashreturn = CityHash32(new_name,len);
	hashresult = (hashreturn % 8000);
	//use the hashresult for the index in the array to store the first byte of
	//where to read from in the mapping file

	//write to mapping file alias and existing name
	int i = open(mapping_file, O_RDWR);
	sprintf(buffer2, "%s %s\n", existing_name, new_name);
	pwrite(i, buffer2, 128, map_offsest);
	map_array[hashresult] = map_offsest;
	//printf("%s%d%s%d%s%s\n","value in array: ", map_array[hashresult], "at position: ", hashresult, "for alias name: ", new_name);
	map_offsest += 128;
	
	sprintf(header1, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
	send(clp, header1, strlen(header1), 0);
	free(buffer);
	free(buffer2);
	close(clp);
	return;
}

void getfunc(char resourse_name[28], int clp){
	int FD = 0;
	int counter = 0;
	char head [55];
	char alias [28];
	char pointer [28];
	char *buffer1 = (char*)malloc(1024);
	char *bad_request = (char*)"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";
	char *doesnt_exist = (char*)"HTTP/1.1 404 File Not Found\r\nContent-Length: 0\r\n\r\n";
	char *ok = (char*)"HTTP/1.1 200 OK\r\nContent-Length:";
	if(strlen(resourse_name)== 27){
		//it is not an alias
	}else{
		//it is a possible alias
		//printf("%s%s%s\n", "in possible alias and ", "resourse_name = ", resourse_name);
		size_t len = strlen(resourse_name);
		uint32_t hashreturn = CityHash32(resourse_name,len);
		int hashresult = (hashreturn % 8000);
		//printf("%s%s%s%d\n", "hashresult of: ", resourse_name, " is ", hashresult);
		int index = map_array[hashresult];
		//printf("%s%d\n","index = ", index );
		if(index==0){
			sprintf(head, bad_request, strlen(bad_request), 0);
			send(clp, head, strlen(head), 0);
			return;
		}
		int i = open(mapping_file, O_RDWR);
		pread(i, buffer1, 128, index);
		sscanf(buffer1, "%s %s\n", pointer, alias);
		//keeps going through aliases until it finds a resource name
		while(strlen(pointer)!=27){
			//checks if its in a loop following aliases
			if(counter == 9){
				sprintf(head, bad_request, strlen(bad_request), 0);
				send(clp, head, strlen(head), 0);
				return;
			}
			len = strlen(pointer);
			hashreturn = CityHash32(pointer,len);
			hashresult = (hashreturn % 8000);
			//printf("%s%d%s%s%s\n", "checking array position: ", hashresult, "to see if ", pointer, " is an alias");
			if(map_array[hashresult]==0){
				sprintf(head, bad_request, strlen(bad_request), 0);
				send(clp, head, strlen(head), 0);
				return;
			}else{
				//we have the alias in the array
				index = map_array[hashresult];
				pread(i, buffer1, 128, index);
				sscanf(buffer1, "%s %s\n", pointer, alias);
				//printf("%s%s%s%s%s\n", "pulled: ", alias, " and ", pointer, " from map file");
				counter += 1;
			}
		}
		free(buffer1);
		resourse_name = pointer;
		
	}

	

	for(size_t i=0; i<strlen(resourse_name);i++){
		if(isalpha(resourse_name[i])==0 && isdigit(resourse_name[i])==0 && resourse_name[i] != '_' && resourse_name[i] != '-'){
			sprintf(head, bad_request, strlen(bad_request), 0);
			send(clp, head, strlen(head), 0);
			return;
		}
	}
	//open the file you want to get
	if(access(resourse_name, F_OK) != -1){
		//file exists
		FD = open(resourse_name, O_RDWR);
		if(FD == -1){
			sprintf(head, doesnt_exist, strlen(doesnt_exist), 0);
			send(clp, head, strlen(head), 0);
			return;
		}
	}else{
		//file doesn't exist use code 404
		sprintf(head, doesnt_exist, strlen(doesnt_exist), 0);
		send(clp, head, strlen(head), 0);
		return;
	}
	//gets file length to use for content-length
	long content = filelength(resourse_name);
	//figures out how many digits content is so we can print out the header
	//int nDigits = floor(log10(abs(content)))+1;
	//print header strlen(ok)+nDigits, 0
	sprintf(head, "%s %ld\r\n\r\n",  ok, content);
	send(clp, head, strlen(head), 0);
	//code from dog to read a file and print
	char *buffer = (char*)malloc(1024);
	int bytesRead = read(FD,buffer,1024);
	while(bytesRead>0){
		int w = write(clp,buffer,bytesRead);
		if(w==-1){
			warn("%s\n", "write error");
		}
		bytesRead = read(FD,buffer,1024);
	}
	free(buffer);
	close(FD);

	return;
}

void putfunc(char resourse_name[28], int32_t clp, int cLen){
	//if the resource doesn't already exist on your server it's created (201) and if it already
	//exists overwrite it (200)
	char alias [28];
	char pointer [28];
	char *buffer1 = (char*)malloc(1024);
	int counter = 0;
	int32_t FD = 0;
	int create_overwrite = 0;
	char head [55];
	//char header [55];
	char header1 [55];
	//char *doesnt_exist = (char*)"HTTP/1.1 404 File Not Found\r\nContent-Length: 0\r\n\r\n";
	char *bad_request = (char*)"HTTP/1.1 400 Bad Request\r\nContent-Length: 0\r\n\r\n";

	//if the resource name is not 27 characters long we assume it is an alias
	if(strlen(resourse_name)== 27){
		//it is a file name so we can continue
	}else{
		size_t len = strlen(resourse_name);
		uint32_t hashreturn = CityHash32(resourse_name,len);
		int hashresult = (hashreturn % 8000);
		int index = map_array[hashresult];
		if(index==0){
			sprintf(head, bad_request, strlen(bad_request), 0);
			send(clp, head, strlen(head), 0);
			return;
		}
		int i = open(mapping_file, O_RDWR);
		pread(i, buffer1, 128, index);
		sscanf(buffer1, "%s %s\n", pointer, alias);
		//keeps going through aliases until it finds a resource name
		while(strlen(pointer)!=27){
			//checks if its in a loop following aliases
			if(counter == 9){
				sprintf(head, bad_request, strlen(bad_request), 0);
				send(clp, head, strlen(head), 0);
				return;
			}
			len = strlen(pointer);
			hashreturn = CityHash32(pointer,len);
			hashresult = (hashreturn % 8000);
			if(map_array[hashresult]==0){
				sprintf(head, bad_request, strlen(bad_request), 0);
				send(clp, head, strlen(head), 0);
				return;
			}else{
				//we have the alias in the array
				index = map_array[hashresult];
				pread(i, buffer1, 128, index);
				sscanf(buffer1, "%s %s\n", pointer, alias);
				counter += 1;
			}
		}
		free(buffer1);
		resourse_name = pointer;
	}

	for(size_t i=0; i<strlen(resourse_name);i++){
		if(isalpha(resourse_name[i])==0 && isdigit(resourse_name[i])==0 && resourse_name[i] != '_' && resourse_name[i] != '-'){
			sprintf(head, bad_request, strlen(bad_request), 0);
			send(cl, head, strlen(head), 0);
			return;
		}
	}
	//int rec = recv(cl, buffer, 1024, 0);
	//code from dog to write to a file
	FD = open(resourse_name, O_RDWR | O_TRUNC);
	//if the file does not exist
	if(FD == -1){
		//file needs to be created
		if(errno == 2){
			FD = open(resourse_name, O_CREAT | O_RDWR | O_TRUNC | O_NONBLOCK, 0666);
			create_overwrite = 201;
		}else if(errno == 13){
			//forbidden file
			sprintf(header1, "HTTP/1.1 403 Forbidden\r\nContent-Length: 0\r\n\r\n");
			send(clp, header1, strlen(header1), 0);
			return;
		}else if(errno == 21){
			//directory
			sprintf(header1, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
			send(clp, header1, strlen(header1), 0);
			return;
		}else if(errno == 13){
			//non readable file
			sprintf(header1, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
			send(clp, header1, strlen(header1), 0);
			return;
		}else{
			//some other error
			sprintf(header1, "HTTP/1.1 500 Internal Server Error\r\nContent-Length: 0\r\n\r\n");
			send(clp, header1, strlen(header1), 0);
			return;
		}
	}else{
		create_overwrite = 200;
	}

	char *buffer = (char *)malloc(cLen * sizeof(char));
	int bytesRead = recv(clp,buffer,cLen,0);
	int w = write(FD,buffer,bytesRead);
	if(w == -1){
		warn("%s\n", "write error");
	}
	//bytesRead = recv(cl,buffer,1024,0);
	//printf("%s\n", buffer2);
	if(create_overwrite == 200){
		sprintf(header1, "HTTP/1.1 200 OK\r\nContent-Length: 0\r\n\r\n");
		send(clp, header1, strlen(header1), 0);
	}else{
		sprintf(header1, "HTTP/1.1 201 File Created\r\nContent-Length: 0\r\n\r\n");
		send(clp, header1, strlen(header1), 0);
	}
	free(buffer);
	close(FD);
	//printf("%s",buffer);
	//free(buffer);
	return;
}

//header is sent here to be parsed
void parsehead(int32_t clp){
	char *buffer = (char*)malloc(1024);
	char *pointer= buffer;
	char header [55];
	char put_get_patch [20];
	char resourse_name [28];
	char find_cLen [28];
	char found_cLen [28];
	//int cLenPut = -1;
	int offset;
	int cLen = 0;

	int bytesRead = read(clp,buffer,1024);
	if (bytesRead == -1){
		warn("%s\n", "read error");
	}
	sscanf(buffer, "%s %s %s %n\n", put_get_patch, resourse_name, find_cLen, &offset);
	if((strcmp(put_get_patch,"PUT")==0) || (strcmp(put_get_patch, "PATCH")==0)){
		//finds the content length in header
		while(strcmp(find_cLen,"Content-Length:")!=0){
			pointer += offset;
			sscanf(pointer, "%s%n", find_cLen, &offset);
		}
		pointer += offset;
		sscanf(pointer, "%s", found_cLen);
		sscanf(found_cLen, "%d", &cLen);

	}

	//checks for leading slash and removes it if there is
	if(resourse_name[0] == '/'){
		memmove(resourse_name, resourse_name+1,strlen(resourse_name));
	}
	//std::cout << put_get << "\n";
	if(strcmp(put_get_patch,"GET")==0){
		getfunc(resourse_name, clp);
	}else if(strcmp(put_get_patch,"PUT")==0){
		putfunc(resourse_name, clp, cLen);
	}else if(strcmp(put_get_patch, "PATCH")==0){
		patchfunc(resourse_name, clp, cLen);
	}else{
		sprintf(header, "HTTP/1.1 400 Bad Request\r\nContent Length: 0\r\n\r\n");
	}
	free(buffer);
	return;
}

//worker thread
//void *worker_thread(void * thread_id){
void *worker_thread(void * thread_id){
	thread_id = NULL;
	//int tid = (int)(ssize_t)thread_id;
	//printf("%d%s\n", tid," Thread initialized" );
	while(1){
		pthread_cond_wait(&cond, &lock2);
		//thread has been signaled by dispacter thread to do work
		//printf("%s%d%s\n", "Thread ", tid, " is awakened");
		pthread_mutex_lock(&lock1);
		//takes first item in the queue and pops it 
		//calls parseheader with what it popped
		int32_t front = cl_queue.front();
		cl_queue.pop();
		pthread_mutex_unlock(&lock1);
		//calls for header to be parsed
		parsehead(front);
		//cl_vector[tid] = 0;
		//printf("%s\n", "signaling dispactcher");
		pthread_cond_signal(&cond1);
		//sleep(1);
		//setting the position in the array back to 0
		//to indicate that the worker thread is done working
		//to the main thread
		/*pthread_mutex_lock(&lock1);
		cl_array[tid] = 0;
		printf("%s\n", "setting array position back to 0");
		pthread_mutex_unlock(&lock1);*/
		//signaling dispactcher that the thread is finished

		//return(NULL);
	}
}

int main(int argc, char *argv[]){
		int opt_num;
		int islog_flag = 0;
		int is_a = 0;
		char* log_file;
		int num_threads = 4;
		size_t len = 0;
		uint32_t hashreturn = 0;
		int hashresult = 0;
		char pointer [55];
		char alias [55];
		char *buffer = (char*)malloc(1024);
		const char * hostname;
		const char * port;

		//finds flags in the command line
		//found off of piazza post
		while((opt_num = getopt(argc, argv, "a:N:l:")) != -1){
			switch(opt_num){
				case 'a':
					mapping_file = optarg;
					is_a = 1;
				case 'N':
					num_threads = atoi(optarg);
					break;
				case 'l':
					log_file = optarg;
					islog_flag = 1;
					break;
				case '?':
					break;	
			}
		}
		if(is_a != 1){
			warn("%s\n", "-a required");
			return 0;
		}
		if(num_threads == 0){
			num_threads = 4;
		}
		//finds the hostname and port in the command line whether there is 
		//or isn't flags
		if(argc - optind == 2){
			hostname = argv[optind];
			port = argv[optind+1];
		}else{
			hostname = argv[1];
			port = argv[2];
		}
		//see if mapping file exists or not
		int FD = open(mapping_file, O_RDWR, 0644);
		//if the file does not exist
		if(FD == -1){
			//file needs to be created
			if(errno == 2){
				FD = open(mapping_file, O_CREAT | O_RDWR | O_TRUNC | O_NONBLOCK, 0666);
			}
		}else{
			//add everything from mapping file into array
			int32_t bytesRead = pread(FD, buffer, 128, map_offsest);
			while(bytesRead > 0){
				//gets alias from map file and puts it in our array
				sscanf(buffer, "%s %s\n", pointer, alias);
				len = strlen(alias);
				hashreturn = CityHash32(alias,len);
				hashresult = (hashreturn % 8000);
				map_array[hashresult] = map_offsest;
				map_offsest += 128;
				bytesRead = pread(FD, buffer, 128, map_offsest);
			}
		}
		//code provided from piazza
		struct addrinfo *addrs, hints = {};
		hints.ai_family = AF_INET;
		hints.ai_socktype = SOCK_STREAM;
		getaddrinfo(hostname, port, &hints, &addrs);
		int32_t main_socket = socket(addrs->ai_family,addrs->ai_socktype,addrs->ai_protocol);
		if (main_socket == -1){
			warn("%s", "socket error");
			return 1;
		}
		int enable = 1;
		setsockopt(main_socket, SOL_SOCKET, SO_REUSEADDR,&enable, sizeof(enable));
		bind(main_socket,addrs->ai_addr, addrs->ai_addrlen);
		//N is the maximum number of "waiting" connections on the socket.
		//we suggest something like 16
		uint8_t N = 16;
		int8_t li = listen(main_socket, N);
		if (li == -1){
			warn("%s", "listen error");
			return 1;
		}
		//initialize worker threads
		pthread_t *thread_id = (pthread_t *)malloc(sizeof(pthread_t) * num_threads);
		for(int i = 0; i < num_threads; i++){
			//int rc = pthread_create(&thread_id[i], NULL, worker_thread, (void *)i);
			int rc = pthread_create(&thread_id[i], NULL, worker_thread, NULL);
			if( rc < 0){
				warn("%s", "pthread_create error");
				return 1;
			}
		}
	//int workers_busy = 0;

	//--------------Dispatcher thread------------------
	while(1){
		//your code, starting with accept(), goes here
		cl = accept(main_socket, NULL, NULL);
		if (cl == -1){
			warn("%s", "accept error");
			return 1;
		}
		pthread_mutex_lock(&lock1);
		//printf("%lu\n", cl_queue.size());
		//checks to see if the workers are all busy
		if((int)(cl_queue.size())==num_threads){
			pthread_mutex_unlock(&lock1);
			pthread_cond_wait(&cond1, &lock3);
			pthread_mutex_lock(&lock1);
		}
		cl_queue.push(cl);
		pthread_mutex_unlock(&lock1);
		//signals a worker thread that there is work to be done
		pthread_cond_signal(&cond);
		/*
			int z = 0;
			while(cl_array[i]!=0){
				printf("%s\n", "in while loop");
				if(z>num_threads-1){
					pthread_mutex_unlock(&lock1);
					printf("%s\n", "All workers are busy so dispacter is going to sleep");
					pthread_cond_wait(&cond1, &lock3);
					pthread_mutex_lock(&lock1);
					printf("%s\n", "dispatch woken up!!");
					return 1;
				}
				z++;
				i++;
				if(i>num_threads-1){
					i = 0;
				}
			}
			*/
	}
	//close(cl);
	
}
