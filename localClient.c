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
#include <regex.h>

#define SUCCESS 	0
#define ERROR 		-1
#define TRUE 		1
#define FALSE 		0
#define DEF_PORT	80
#define INVALID_URL			-1 
#define VALID_NO_PORT		0
#define VALID_WITH_PORT		1
#define STREAM_SIZE			1024
#define RFC1123FMT "%a, %d %b %Y %H:%M:%S GMT"

int validUsage(int argc, char** argv, int* h, int* d, char** date, char** url);
int validInput(int d, char* date, char* url, int* port, char** host, char** path, int* day, int* hour, int* min);
int validURL(char* url);
int openSocket(int port, char* host);
int sendRequest(char* request, int destfd);
char* dataStream(int sourcefd);
char* constructRequest(int h,int d,char* date, char* host, char* path);
char* getTime(int day, int hour, int min);
void freeMemory(char* host, char*  path, char* date, char* request, char* response);


int main(int argc, char* argv[])
{	//----------Variables-----------
	int h = FALSE, d = TRUE, day = 0, hour = 0, min = 0;				//Flags
	int socketfd, port = DEF_PORT;
	char* url;
	char* request;//= argv[1];
	char* response;// = "yes";
	char* date ;//= "Sat, 29 Oct 1994 19:43:31 GMT";
	char* host ;//= "rare-hub-382.appspot.com";
	char* path ;//= "/";
	
	if(validUsage(argc, argv, &h, &d, &date, &url) == FALSE) 
	{
		fprintf(stderr , "Usage: client [-h] [-d <timeinterval>] <URL>\n");
		return ERROR;
	}
	if(validInput(d, date, url,&port, &host, &path, &day, &hour, &min) == FALSE)
	{
		freeMemory(host, path, NULL, NULL, NULL);		fprintf(stderr, "wrong input\n");
		return ERROR;	}
	date = getTime(day, hour, min);
	
	//Assuming the input is valid now

	//Open socket and connect to server
	socketfd = openSocket(port, host);
	if(socketfd == ERROR)
	{		freeMemory(host, path, date, NULL, NULL);
		return ERROR;	}
	
	//Construct request
	request = constructRequest(h, d, date, host, path);
	
	printf("HTTP request =\n%s\nLEN = %d\n", request, strlen(request));
	
	//Send request
	if(sendRequest(request, socketfd) == ERROR)
	{
		freeMemory(host, path, date, request, NULL);
		return ERROR;
	}
	
	//Read server response from socket
	response = dataStream(socketfd);
	if(response == NULL)
	{		freeMemory(host, path, date, request, NULL);
		return ERROR;	}
	printf("\n---\n%s\n---\n", response);
	printf("\n Total received response bytes: %d\n",strlen(response));
	
//	freeMemory(host, path, date, request, response);
	close(socketfd);
	return TRUE;
}

//================Input Check================
int validUsage(int argc, char** argv, int* h, int* d, char** date, char** url)
{	if(argc != 2 && argc != 3 && argc != 4 && argc != 5)	
	//Check input length for (every combination) + 1
		return FALSE;
	
	//check make sure the format is right
	int i = 0;	
	*h = FALSE;
	*d = FALSE;
	*date = NULL;
	*url = NULL;
	
	for(i = 1; i < argc; i++)
	{		if((strcmp(argv[i], "-h") == 0) && (*h == FALSE))
		{//found "-h" flag			
			*h = TRUE;
		}	
		else
		{			if((strcmp(argv[i], "-d") == 0) && (*d == FALSE))
			{//found "-d" flag				
				*d = TRUE;
				//locate date
				if(i+1 < argc)
				{					*date = argv[i+1];
					i++;				}
				else
					return FALSE;
			}			
			else
			{				if(*url == NULL)
					*url = argv[i];
				else
					return FALSE;			}	
		}	}
	#ifdef TEST
		if(*d)	
			printf("d Flag on\n");
		if(*date != NULL)
			printf("date: %s\n", *date);
		if(*h)	
			printf("h Flag on\n");
		if(*url != NULL)
			printf("url: %s\n", *url);
	
	#endif
	//Url is required 
	if(*url == NULL)
		return FALSE;
			return TRUE;	}

int validInput(int d, char* date, char* url, int* port, char** host, char** path, int* day, int* hour, int* min)
{
	*host = (char*)malloc(sizeof(char) * strlen(url));
	*path = (char*)malloc(sizeof(char) * strlen(url));
	if(d)
	{		if(sscanf(date, "%d:%d:%d", day, hour, min) != 3)
			return FALSE;	}
	int urlCheck = validURL(url);
	if(urlCheck == INVALID_URL){
		return FALSE;
	}

	
	
	if(urlCheck == VALID_WITH_PORT)
	{		if(sscanf(url, "http://%[^:]:%d/%[^\n]", *host, port, *path) != 3)			*path = "/";	}
	else
	{		*port = DEF_PORT;		if(sscanf(url, "http://%[^/]/%[^\n]", *host, *path) != 2)
			strcpy(*path , "/");		}			
	return TRUE;}

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
	server = (struct hostent *)gethostbyname("localhost");
	//server = (struct hostent *)gethostbyname(host);
	if (server == NULL) 
	{
		herror("gethostbyname\n");
		return ERROR;
	}
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = PF_INET;
	bcopy((char *)server->h_addr, 
			(char *) &serv_addr.sin_addr.s_addr, 
			server->h_length);
	serv_addr.sin_port = htons(port);			//Converts between host byte order and network byte order

	if (connect(sockfd,(struct sockaddr *)&serv_addr,sizeof(serv_addr)) < 0)
	{		perror("ERROR connecting");
		return ERROR;	}
		
	return sockfd;
}

//================Read data from stream================
//reads ALL data from source 
char* dataStream(int sourcefd)
{		char* result = (char*)malloc(sizeof(char)*(STREAM_SIZE+1));
	int potentialLength = STREAM_SIZE;
	int actualLength = 0;
	int	readResult = 0;
	
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
			result = (char*)realloc(result, sizeof(char)*(potentialLength+1));		}	}while(readResult != 0 && readResult != -1);
	result[actualLength] = '\0';
	return result;}

//================Write data to stream================
int sendRequest(char* request, int destfd)
{		int sent = 0;
	int totalSent = 0;
	int length = strlen(request);
	while(totalSent < length)
	{		sent = write(destfd, &request[totalSent], length-totalSent);		
		totalSent = totalSent + sent;
		if(sent < 0)
		{			perror("ERROR writing to socket");			return ERROR;		}	}		
	return SUCCESS;}

//================Construct HTTP request================
char* constructRequest(int h,int d,char* date, char* host, char* path)
{//	"GET path HTTP/1.0\r\n\r\n";
	int	resultLength = 0, index = 0;
	char type [6];
	if(h)
		strcpy(type, "HEAD ");
	else
		strcpy(type, "GET ");
	char protocol [] = " HTTP/1.0";
	char hostTitle [] = "\nHost: ";
	char connection [] = "\nConnection: close";
	char modifiedTitle [] = "\nIf-Modified-Since: ";
	char endLine [] = "\r\n\r\n";
	resultLength = strlen(type) + strlen (path) + strlen(protocol) + strlen(hostTitle) + strlen(host) + strlen(connection) + strlen(modifiedTitle) +strlen(endLine);
	
	if(d)
		resultLength = resultLength + strlen(date);
		char* result = (char*)malloc(sizeof(char)*resultLength+1);
	
	strcpy(&result[index], type);
	index = index + strlen(type);
		
	strcpy(&result[index], path);
	index = index + strlen(path);
	
	strcpy(&result[index], protocol);
	index = index + strlen(protocol);
	
	strcpy(&result[index], hostTitle);
	index = index + strlen(hostTitle);

	strcpy(&result[index], host);
	index = index + strlen(host);
	
	strcpy(&result[index], connection);
	index = index + strlen(connection);
		
	
	if(d)
	{		strcpy(&result[index], modifiedTitle);
		index = index + strlen(modifiedTitle);
		strcpy(&result[index], date);
		index = index + strlen(date);		}	
	
	strcpy(&result[index], endLine);
	index = index + strlen(endLine);

	
	result[resultLength] = '\0';
	return result;}


//================URL Validation================
//return:	-1 	= invalid url
//			0 	= valid, NO port
//			1 	= valid, WITH port
int validURL(char* url)
{	
    int portCase;
    int noPortCase;
    regex_t regex_1, regex_2;	
	//---Building regular expression---
    // "\\<" 	= start with
    // "[^:]" 	= everything but:    // "+" 		= 1 or more    // "?"		= 0 or 1
    // "[]"		= group of
    // "[:digit:]" = digit
    if(url[(strlen(url)-1)] != '/')
    	strcat(url, "/");
    
    regcomp(&regex_1, "\\<http:\\/\\/([^:]+):[[:digit:]]+\\/", REG_EXTENDED);
    regcomp(&regex_2, "\\<http:\\/\\/([^\\/:]+)\\/", REG_EXTENDED);
	//---Running the URL on that expression---
	portCase = regexec(&regex_1, url, 0, NULL, REG_EXTENDED);
	noPortCase = regexec(&regex_2, url, 0, NULL, REG_EXTENDED);    
	
	regfree(&regex_1);
	regfree(&regex_2);

	if(portCase == SUCCESS){		#ifdef TEST			printf("WITH\n");
		#endif
		return VALID_WITH_PORT;
	}

	if(noPortCase == SUCCESS){
		#ifdef TEST
			printf("NO PORT\n");
		#endif
		return VALID_NO_PORT;
	}
	#ifdef TEST
		printf("INVALID\n");
	#endif
	return INVALID_URL;}


void freeMemory(char* host, char*  path, char* date, char* request, char* response)
{	if(host != NULL)
		free(host);
	if(path != NULL)
		free(path);
	if(date != NULL)
		free(date);
	if(request != NULL)
		free(request);
	if(response != NULL)		
		free(response);}






