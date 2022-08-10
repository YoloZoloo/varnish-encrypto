# varnish-encrypto

The initial intention of writing this piece of code was to implement backend tls encryption for open source varnish, since Varnish Cache lacks this feature.

Now it is more of just a personal project.

There is cmake file so that you should be able to compile the source code using it, with caveat that necessary library paths should be adjusted according to the openssl, pthread library in your settings.

## What does this code do?

It will receive unencrypted HTTP request from a client then encrypt it and send that request to the backend.
Internally, it has a thread pool implemented for multithreading and backend connection reuse. The thread pool is a very naive one, it is a linked list of nodes waiting for task.
When there is a task to be assigned, acceptor thread will check availability in the thread pool and if there is any, acceptor thread will wake up a first available thread.
After thread is done handling the request, it will queue itself to the end of the linked list.

## More about thread pool
There are 3 types of threads that are related to thread pool. First kind is the worker thread. Worker threads do the actual heavy lifting of handling request. Then there is acceptor thread. Acceptor thread is dedicated to only accepting incoming connections from client.
Third and the last one is connection monitoring thread. It is not fully implemented yet. But after implementation it should watch over idle connections and queue the expired connection back to inactive pool( details below ).
There are 2 queues in the thread pool, idle connection queue and inactive connection queue. The name is self explanatory, idle queue is for queueing ESTABLISHED connections and the latter is for CLOSED connections.

## Backend connection pooling
The thread pool queues worker threads to inactive pool or idle pool, depending on the backend connection status. If transaction is complete and the connection is alive, we want to reuse that connection to avoid the cost of TLS handshake, thus ESTABLISHED connections must be queued to idle pool. If the socket for that particular connection received FIN it will close itself and the worker thread handling that request will be queued to inactive queue.

# Building from Source Code
Go to the top directory and run following commands.

```bash
cmake -B build
cmake --build build
```
