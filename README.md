# varnish-encrypto

The initial intention of writing this piece of code was to implement backend TLS encryption for open-source varnish since Varnish Cache lacks this feature.

Now it is more of just a personal project.

There is a CMake file, so you should be able to compile the source code using it, with the caveat that you should adjust the necessary paths according to the OpenSSL and pthread library in your settings.

## What does this code do?

It will receive an unencrypted HTTP request from a client, then encrypt it and send that request to the backend.
Internally, it has a thread pool implemented for multithreading and backend connection reuse. The thread pool is a very naive one; it is a linked list of nodes waiting for a task.
When there is a task to be assigned, the acceptor thread will check availability in the thread pool, and if there is any, the acceptor thread will wake up the first available thread.
After the thread finished handling the request, it will queue itself to the end of the linked list.

## More about thread pool
There are 2 types of threads that are related to thread pool. First kind is the worker thread. Worker threads do the actual heavy lifting of handling request. 
Then there is acceptor thread. Acceptor thread is dedicated to only accepting incoming connections from client.

## Backend connection pooling
The thread pool queues worker threads back to the linked list without closing the ESTABLISHED connection. If the transaction is complete and the connection is alive, we want to reuse that connection to avoid the cost of recurring TLS handshake. Thus, ESTABLISHED connections must be queued to the linked list of threads (pool) without closing the connection. 

# Building from Source Code
Go to the top directory and run following commands.

```bash
cmake -B build
cmake --build build
```

# Executing the binary
From the top directory run 
```bash
build/varnish-encrypto $PORT_ON_HOST $BACKEND_IP_OR_DOMAIN $BACKEND_SERVER_PORT
```
