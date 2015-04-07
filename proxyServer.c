//Name: 	Shahaf Zada
//ID:		203307756
//#define TEST

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <time.h>

#include <pthread.h>
#include <arpa/inet.h>
#include "threadpool.h"

#define SUCCESS 	0
#define ERROR 		-1
#define TRUE 		1
#define FALSE 		0
#define REPEAT_JOB	3
#define STREAM_SIZE	1024
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"
#define DEF_PORT	80
#define NUM_OF_ERRORS 5
#define SUBNET_MASKS	33
#define NUM_OF_CLIENTS	5

#define BAD_REQUEST 	400
#define NOT_FOUND	 	404
#define NOT_SUPPORTED	501


//-----Enums-----
enum msgType{
	badRequest		= 0,
	forbidden 		,
	notFound		,
	internalServer 	,
	notSupported};

//-----Global Varibles-----
char* errorMsgs [NUM_OF_ERRORS];
char* errorMsgsBody [NUM_OF_ERRORS];
char* errorMsgsHead [NUM_OF_ERRORS];

char** hostName = NULL;
char** hostIP = NULL;

unsigned int subnetMask[SUBNET_MASKS];

//-----Function Decleration-----
int validUsage(int argc, char** argv, int* port, int* numOfThreads, int* maxRequests, char** filterPath);
int validInput(char* request, char** host, char** path);
int loadData();
int loadHosts(char* path);
unsigned int parseIPV4string(char* ipAddress);
int hostAfterMask(char* host, int mask);
int openServerSocket(int port);
int openSocket(int port, char* host);
int handleClient(void *arg);
char* dataStream(int sourcefd, int isSourceClient);
int writeData(int destfd, char* data);
char* constructErrorMsg(int errorNum);
char* getTime(int day, int hour, int min);
void destroyServer();

void printArray(char** array);
void printAllErrors();

//"Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n"
int main(int argc, char* argv[])
{	//----------Variables-----------
	int numOfThreads = 0, i = 0;
	int mainSocket = 0, newSocket = 0, port = 0, maxRequests = 0;
	char* filterPath;
	
	//-----Input validation-----
	if(validUsage(argc, argv, &port, &numOfThreads, &maxRequests, &filterPath) == FALSE)
	{
		fprintf(stderr , "Usage: proxyServer <port> <pool-size> <max-number-of-request> <filter>\n");
		return ERROR;
	}
	
	//-----Load data-----
	if(loadData() == ERROR || loadHosts(filterPath) == ERROR)
	{		fprintf(stderr, "Error loading filter hosts\n");
		return ERROR;	}

	//-----Server creates pool of threads, threads wait for jobs.-----
	threadpool* myPool = create_threadpool(numOfThreads);
	if(myPool == NULL){		fprintf(stderr, "Error creating the threadpool :(\n");
		return ERROR;	}
	
	//-----Open the main socket-----
	mainSocket = openServerSocket(port);
	if(mainSocket == ERROR)
	{		destroyServer();
		return ERROR;	}
		
	//-----Accept new connections from clients-----
	for(i = 0; i < maxRequests; i++)
	{
		newSocket = accept(mainSocket, NULL, NULL);
		if (newSocket < 0){			perror("ERROR on accept");
			destroyServer();
			return ERROR;		}			
		dispatch(myPool,handleClient, (void*)newSocket);
	}

	//-----Close porgram-----
	destroy_threadpool(myPool);
	destroyServer();
	close(mainSocket);
	return SUCCESS;}

//================Input Check================
int validUsage(int argc, char** argv, int* port, int* numOfThreads, int* maxRequests, char** filterPath)
{	
	if(argc != 5)
		return FALSE;
	
	if(sscanf(argv[1], "%d", port) != 1)
		return FALSE;
	if(sscanf(argv[2], "%d", numOfThreads) != 1)
		return FALSE;
	if(sscanf(argv[3], "%d", maxRequests) != 1)
		return FALSE;	

	//TODO check if fopen 
	*filterPath = argv[4];		
	return TRUE;}

//================Socket creation================
//Opens a socket, bind it to a port, and listen for requests
//returns a valid socket, ready for reading and writing 
int openServerSocket(int port) 
{
	int socketfd;
	struct sockaddr_in serv_addr;

	//---Open socket---
	//PF_INET - protocol family for the Internet (TCP/IP)
	//SOCK_STREAM , IPPROTO_TCP - for TCP connection
	socketfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (socketfd < 0)
	{		
		perror("ERROR opening socket\n");	
		return ERROR;
	}
	
	//---Initialize server structure---
	bzero((char *) &serv_addr, sizeof(serv_addr));	//serv_addr = 0
	serv_addr.sin_family = PF_INET;					//PF_INET - protocol family for the Internet (TCP/IP)
	serv_addr.sin_addr.s_addr = INADDR_ANY;			//Bind socket to all local interfaces
	serv_addr.sin_port = htons(port);

	//---Bind the server, to the socket---
	if (bind(socketfd, (struct sockaddr *)&serv_addr,	sizeof(serv_addr)) < 0)
	{
		perror("ERROR on binding");
		return ERROR;
	}
	//---Listen to requests on the given port--
	listen(socketfd,NUM_OF_CLIENTS);
		
	return socketfd;
}

//================Read data from stream================
//reads ALL data from source 
//Note: the data MUST end with "\r\n\r\n"
char* dataStream(int sourcefd, int isSourceClient)
{		char* result = (char*)malloc(sizeof(char)*(STREAM_SIZE+1));
	int potentialLength = STREAM_SIZE;
	int actualLength = 0;
	int	readResult = 0;
	char* end = NULL;
	do
	{		readResult = read(sourcefd, &result[actualLength], potentialLength-actualLength);
		if (readResult < 0)
		{			perror("ERROR reading from socket");
			free(result);
			return NULL;		}	
		actualLength = actualLength + readResult;
		
		if(actualLength == potentialLength)				
		{//expend		
			potentialLength = potentialLength + STREAM_SIZE;
			result = (char*)realloc(result, sizeof(char)*(potentialLength+1));		}
		if(isSourceClient)
			if(strstr(result, "\r\n\r\n") != NULL)
				break;			}while(readResult > 0);
	result[actualLength] = '\0';
	return result;}

//================Write data to stream================
int writeData(int destfd, char* data)
{		int sent = 0;
	int totalSent = 0;
	int length = strlen(data);
	while(totalSent < length)
	{		sent = write(destfd, &data[totalSent], length-totalSent);		
		totalSent = totalSent + sent;
		if(sent < 0)
		{			perror("ERROR writing to socket");			return ERROR;		}	}		
	return SUCCESS;}


//================Load Error Masseges================
//	badRequest		=0,
//	forbidden 		,
//	notFound		,
///	internalServer 	,
//	notSupported
int loadData(){	//Bad Request
	errorMsgsHead[badRequest] = "400 Bad Request\r\n";							
	errorMsgsBody[badRequest] = "<HTML><HEAD><TITLE>400 Bad Request</TITLE></HEAD>\r\n"
								"<BODY><H4>400 Bad request</H4>\r\n"
								"Bad Request.\r\n"
								"</BODY></HTML>\r\n";

	//Forbidden
	errorMsgsHead[forbidden] = 	"403 Forbidden\r\n";
	errorMsgsBody[forbidden] = 	"<HTML><HEAD><TITLE>403 Forbidden</TITLE></HEAD>\r\n"
								"<BODY><H4>403 Forbidden</H4>\r\n"
								"Access denied.\r\n"
								"</BODY></HTML>\r\n";

	//Not Found
	errorMsgsHead[notFound] = 	"404 Not Found\r\n";
	errorMsgsBody[notFound] = 	"<HTML><HEAD><TITLE>404 Not Found</TITLE></HEAD>\r\n"
								"<BODY><H4>404 Not Found</H4>\r\n"
								"File not found.\r\n"
								"</BODY></HTML>\r\n";

	//Internal Server
	errorMsgsHead[internalServer] = "500 Internal Server Error\r\n";
	errorMsgsBody[internalServer] = "<HTML><HEAD><TITLE>500 Internal Server Error</TITLE></HEAD>\r\n"
									"<BODY><H4>500 Internal Server Error</H4>\r\n"
									"Some server side error.\r\n"
									"</BODY></HTML>\r\n";

	//Not Supported
	errorMsgsHead[notSupported]	=	"501 Not supported\r\n";
	errorMsgsBody[notSupported]	= 	"<HTML><HEAD><TITLE>501 Not supported</TITLE></HEAD>\r\n"
									"<BODY><H4>501 Not supported</H4>\r\n"
									"Method is not supported.\r\n"
									"</BODY></HTML>\r\n";

	
	
	//-----Initialize Subnet Masks-----
	int j = 1;
	int mask = 1;
	mask = mask<<31;
	subnetMask[j] = mask;
	for(j = 2; j < SUBNET_MASKS; j++)
	{		mask = mask >> 1;		subnetMask[j] = subnetMask[j-1] | mask;	}
	return SUCCESS;
}

//================Change Host Formats================
//Input:	hostIP:	n1.n2.n3.n4	(n is 1 byte number)
//Output:	Host after applying mask
int hostAfterMask(char* hostIP, int mask){
	unsigned int host = parseIPV4string(hostIP);
	return host & subnetMask[mask];
}

//Parse host ip from "n1.n2.n3.n4" to unsigned int form (n1n2n3n4)
unsigned int parseIPV4string(char* ipAddress) {
  int ipbytes[4];
  sscanf(ipAddress, "%d.%d.%d.%d", &ipbytes[3], &ipbytes[2], &ipbytes[1], &ipbytes[0]);
  return ipbytes[0] | ipbytes[1] << 8 | ipbytes[2] << 16 | ipbytes[3] << 24;
}

int loadHosts(char* path){	if(path == NULL)
		return ERROR;
	int hostNameLength = 0, hostIPLength = 0;
	FILE * fd;
	char * line = NULL;
	size_t len = 0;
	ssize_t read;

	fd = fopen(path, "r");
	if (fd == NULL)
	{		perror("fopen");
		return ERROR;
	}
	//Count the number of hosts name, and hosts ip 
	while ((read = getline(&line, &len, fd)) != -1) {		if(read == ERROR)
		{			fprintf(stderr, "getline: Error\n");			free(line);
			fclose(fd);			return ERROR;		}
		if(line[0] != '\n')
		{						//Empty line check
			//Line is not empty			if(line[0] > '9' || line[0] < '0')
			{		//IP check
				//Line is host name
				hostNameLength++;
			}
			else
			{				//Line is host IP
				hostIPLength++;			}		}
	}
	
	//Allocate host arrays
	hostName = (char**)malloc(sizeof(char*) * (hostNameLength+1));
	if(hostName == NULL)
	{		perror("malloc");
		fclose(fd);		return ERROR;	}
	hostName[hostNameLength] = NULL;
	
	hostIP = (char**)malloc(sizeof(char*) * (hostIPLength+1));
	if(hostIP == NULL)
	{		perror("malloc");		free(hostName);		fclose(fd);		return ERROR;	}			
	hostIP[hostIPLength] = NULL;
	
	fseek(fd, 0, SEEK_SET);	//Return to the beginning of the file
	//Fill host arrays
	int nameIndex = 0;
	int IPIndex = 0;
	while ((read = getline(&line, &len, fd)) != -1) {
		if(read == ERROR)
		{			fprintf(stderr, "getline: Error\n");			free(hostName);
			free(hostIP);				free(line);
			fclose(fd);			return ERROR;					}		if(line[0] != '\n')
		{						//Empty line check
			//Line is not empty			if(line[0] > '9' || line[0] < '0')
			{		//IP check
				//Line is host name
				hostName[nameIndex] = strdup(line);
				hostName[nameIndex][read-1] = '\0';
				nameIndex++;
			}
			else
			{				//Line is host IP
				hostIP[IPIndex] = strdup(line);
				hostIP[IPIndex][read-1] = '\0';
				IPIndex++;			}		}
	}		
		
	if (line)
		free(line);		

	fclose(fd);
	return SUCCESS;}

int isHostInFilter(char* host){	if(host == NULL)
		return FALSE;			

	if(isHostInHostName(host))
		return TRUE;
	
	struct hostent *server;	
	server = (struct hostent *)gethostbyname(host);
	if (server == NULL) 
	{
		herror("gethostbyname");
		return ERROR;
	}	
	char* ip = inet_ntoa(*((struct in_addr*) server->h_addr));
	if(isHostInHostIP(ip))
		return TRUE;
	
	
	return FALSE;	}


int isHostInHostName(char* host){	int i = 0;
	for(i = 0; hostName[i] != NULL; i++)
	{		if(strcmp(host, hostName[i]) == 0)
		{			#ifdef TEST
				printf("==host found: %s at:\n",host);
			#endif

			return TRUE;
		}	}
	return FALSE;}

int isHostInHostIP(char* host){	int i = 0;
	unsigned int filterIP, targetIP, mask, temp;
	for(i = 0; hostIP[i] != NULL; i++)
	{		sscanf(hostIP[i], "%d.%d.%d.%d/%d",&temp, &temp, &temp, &temp,&mask);
		filterIP = hostAfterMask(hostIP[i], mask);
		targetIP = hostAfterMask(host, mask);
				
		if(filterIP == targetIP)
		{	
			#ifdef TEST
				printf("==ip found: %s at:\n",host);
				printf("==host in IP: %s\n", hostIP[i]);
			#endif
			return TRUE;
		}	}
	return FALSE;}

//================Handle Client================
int handleClient(void * arg) {	int socketfd = (int)arg;
	int retCode = 0;
	
	//-----Read client request-----
	char* clientRequest = dataStream(socketfd, TRUE);
	if(clientRequest == NULL)
	{		sendErrorMsg(socketfd, internalServer);
		return ERROR;	
	}
		
	//-----Input Check----	
	char* host, *path;
	retCode = validInput(clientRequest, &host, &path);
	//Check for errors
	if(retCode != SUCCESS)
	{		if(retCode == BAD_REQUEST)
		{			free(clientRequest);			sendErrorMsg(socketfd, badRequest);
			return ERROR;			}
		if(retCode == NOT_SUPPORTED)
		{			free(clientRequest);			sendErrorMsg(socketfd, notSupported);
			return ERROR;			}	}
	
	//Check if in filter 
	retCode = isHostInFilter(host);
	if(retCode == TRUE)
	{		free(clientRequest);		free(path);
		free(host);		sendErrorMsg(socketfd, forbidden);
		return ERROR;		}
	if(retCode == ERROR)
	{		free(clientRequest);		free(path);
		free(host);		sendErrorMsg(socketfd, notFound);
		return ERROR;		}
	#ifdef TEST
		printf("HTTP request =\n%s\nLEN = %d\n", clientRequest, strlen(clientRequest));
	#endif
	//if not, connet to dest host
	int hostSocket = openSocket(DEF_PORT, host);	
	if(hostSocket == ERROR)
	{		free(clientRequest);		free(path);
		free(host);		sendErrorMsg(socketfd, internalServer);
		return ERROR;	}
	if(hostSocket == NOT_FOUND)
	{		free(clientRequest);		free(path);
		free(host);		sendErrorMsg(socketfd, notFound);
		return ERROR;	}

	//Send request
	if(writeData(hostSocket, clientRequest) == ERROR)
	{		free(clientRequest);		free(path);
		free(host);		sendErrorMsg(socketfd, internalServer);
		return ERROR;			}

	//Read server response from socket
	char* response = dataStream(hostSocket, FALSE);
	if(response == NULL)
	{		free(clientRequest);		free(path);
		free(host);		sendErrorMsg(socketfd, internalServer);
		return ERROR;
	}
	close(hostSocket);
		
	//Send response
	if(writeData(socketfd, response) == ERROR)
	{		free(clientRequest);
		free(response);		free(path);
		free(host);		sendErrorMsg(socketfd, internalServer);
		return ERROR;			}
	free(clientRequest);
	free(response);	free(path);
	free(host);
	close(socketfd);
	return SUCCESS;
}
//================Input Check================
int validInput(char* request, char** host, char** path){
	if(request == NULL)
		return BAD_REQUEST;
	char* tempBuffer = strdup(request);	
	
	//-----Check first line-----
	char* line = strtok(tempBuffer, "\n");
	if(line == NULL)
	{		free(tempBuffer);
		return BAD_REQUEST;	}

	char *method = (char*)malloc(sizeof(char)*strlen(line));
	char *protocol = (char*)malloc(sizeof(char)*strlen(line));
	*path = (char*)malloc(sizeof(char)*strlen(line));
	if(sscanf(line, "%s %s %s\r\n",method, *path, protocol) != 3)
	{
		free(method);		
		free(protocol);
		free(*path);		free(tempBuffer);
		return BAD_REQUEST;	}
	if(strcmp(method, "GET") != 0)
	{
		free(method);
		free(protocol);
		free(*path);		free(tempBuffer);
		return NOT_SUPPORTED;	}
	if(strcmp(protocol, "HTTP/1.0")!= 0 && strcmp(protocol, "HTTP/1.1")!=0)
	{
		free(method);
		free(protocol);
		free(*path);		free(tempBuffer);
		return BAD_REQUEST;	}

	char* hostTitle;
	int foundHost = FALSE;
	while(line != NULL)
	{		*host = (char*)malloc(sizeof(char) * strlen(line));
		hostTitle = (char*)malloc(sizeof(char) * strlen(line));
					
		if(sscanf(line, "%s %s", hostTitle, *host) == 2 && strcasecmp(hostTitle, "Host:") == 0)		
		{			foundHost = TRUE;			break;		}
				
		free(*host);
		free(hostTitle);	
		*host = NULL;
		hostTitle = NULL;	
		line = strtok(NULL, "\r\n");		}
	free(hostTitle);
	if(foundHost == FALSE)
	{//Means we found no host
		free(method);
		free(protocol);
		free(*path);
		free(tempBuffer);
		free(*host);
		return BAD_REQUEST;	}
	
	free(method);
	free(protocol);
	free(tempBuffer);
	return SUCCESS;}

//================Construct Error Msgs================
char* constructErrorMsg(int errorNum){	char* result;								//Initialize the result for empty string		
	char* date = getTime(0, 0, 0);
	if(date == NULL)
		return NULL;
	asprintf(&result, 	"HTTP/1.0 %s"				//errorMsgsHead[errorNum]
						"Server: webserver/1.0\r\n"	
						"Date: %s\n"				//date
						"Content-Type: text/html\r\n"
						"Content-Length: %d\r\n"	//strlen(errorMsgsBody[errorNum])
						"Connection: close\r\n"
						"\r\n"
						"%s"						//Msg body
						,errorMsgsHead[errorNum], date, strlen(errorMsgsBody[errorNum]), errorMsgsBody[errorNum]);
	free(date);	
	return result;}

//================Send Error Msg================
int sendErrorMsg(int fd,int errorNum){	char* errorMsg = constructErrorMsg(errorNum);
	if(errorMsg == NULL)
		return ERROR;
	
	if(writeData(fd, errorMsg) == ERROR)
		return ERROR;
		
	free(errorMsg);
	close(fd);
	return SUCCESS;}
//================Get time================
char* getTime(int day, int hour, int min)
{	time_t now;
	int timebufSize = 128;
	char* timebuf = (char*)malloc(sizeof(char)*timebufSize);

	now = time(NULL);
	now=now-(day*24*3600+hour*3600+min*60); //where day, hour and min are the values
	//from the input
	strftime(timebuf, sizeof(char)*timebufSize, RFC1123FMT, gmtime(&now));
	//timebuf holds the correct format of the time.
	return timebuf;}

//================Socket creation================
//Opens and connects a socket. 
//returns a valid socket, ready for reading and writing 
int openSocket(int port, char* host) 
{
	int sockfd;
	struct sockaddr_in serv_addr;
	struct hostent *server;	

	//PF_INET - protocol family for the Internet (TCP/IP)
	//SOCK_STREAM , IPPROTO_TCP - for TCP connection
	sockfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sockfd < 0)
	{		
		perror("ERROR opening socket\n");	
		return ERROR;
	}
	server = (struct hostent *)gethostbyname(host);
	if (server == NULL) 
	{
		herror("gethostbyname");
		return NOT_FOUND;
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = PF_INET;
	bcopy((char *)server->h_addr, 
			(char *) &serv_addr.sin_addr.s_addr, 
			server->h_length);
	serv_addr.sin_port = htons(port);			//Converts between host byte order and network byte order
	if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	{		perror("ERROR connecting");
		//exit(1);
		return ERROR;	}
		
	return sockfd;
}


void destroyServer(){	int i = 0 ;
	if(hostName != NULL)
	{		for(i = 0; hostName[i] != NULL; i++)
			free(hostName[i]);
		free(hostName);		}
	
	if(hostIP != NULL)
	{		for(i = 0; hostIP[i] != NULL; i++)
			free(hostIP[i]);
		free(hostIP);				}	}


//**************Testing*******************
void printArray(char** array){	int i = 0 ;
	while(array[i] != NULL){
		printf("%s\n", array[i]);
		i++;
	}}
void printAllErrors(){	int i = 0 ;
	char* errormsg;
	printf("=================\n");
	for(i = 0; i < NUM_OF_ERRORS;i++)	
	{		errormsg = constructErrorMsg(i);
		printf("%s=================\n", errormsg);
		free(errormsg);
	}}
