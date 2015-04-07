# C-Proxy-Server-Threadpool-Local-HTTP-Client

<<<<<<< HEAD
## The project implements a HTTP proxy-server over the TCPIP protocols using multithreading concepts.
=======
## The project implements a HTTP proxy-server over the TCP/IP protocols using multithreading concepts.
>>>>>>> bc5e22f1889da407a31747c9762dd5455ca45521

- Multithreading achieved by using threadpool library that is part of the project.
- A local client is added in order to help testing the program.
- A filter file example is added (this is where you name the forbidden websites)

<<<<<<< HEAD
### Usage

#### Compiling 
 Compile the server using ```gcc proxyServer.c threadpool.c -o proxyServer -lpthread```
 Compile the client using ```gcc localClient.c -o client```

#### Running
 Command line to run the server  ```.proxyServer port pool-size max-number-of-request filter ```
 Command line to run the client  ```.client [–h] [–d time-interval] URL ```
=======
### Usage:

#### Compiling: 
* Compile the server using: ```gcc proxyServer.c threadpool.c -o proxyServer -lpthread```
* Compile the client using: ```gcc localClient.c -o client```

#### Running:
* Command line to run the server: : ```./proxyServer <port> <pool-size> <max-number-of-request> <filter>: ```
* Command line to run the client: : ```./client [â€“h] [â€“d <time-interval>] <URL>: ```
>>>>>>> bc5e22f1889da407a31747c9762dd5455ca45521

