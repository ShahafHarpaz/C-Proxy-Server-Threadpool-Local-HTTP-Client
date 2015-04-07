# C-Proxy-Server-Threadpool-Local-HTTP-Client

The project implements a HTTP proxy-server over the TCP/IP protocols using multithreading concepts.

- Multithreading achieved by using threadpool library that is part of the project.
- A local client is added in order to help testing the program.
- A filter file example is added (this is where you name the forbidden websites)
Usage:

Compiling: 
Compile the server using: gcc proxyServer.c threadpool.c -o proxyServer -lpthread
Compile the client using: gcc localClient.c -o client

Running:
1) Command line to run the server: ./proxyServer <port> <pool-size> <max-number-of-request> <filter>
2) Command line to run the client: ./client [–h] [–d <time-interval>] <URL>

