# varnish-encrypto

The initial intention of writing this piece of code was to implement backend TLS encryption for open-source varnish since Varnish Cache lacks this feature.

Now it is more of just a personal project.

There is a CMake file, so you should be able to compile the source code using it, with the caveat that you should adjust the necessary paths according to the OpenSSL and pthread library in your settings.

## WHY?

Basically two things are addressed.
1. It handles TLS encryption for the client. So the client doesn't need to know how to communicate over TLS.
2. The TLS connections won't be closed and kept open as long as possible. This way we avoid doing the expensive TLS handshake for every request.

## What does this code do?

It receives unencrypted plain HTTP requests from a client, then encrypt these requests to finally relay those requests to the backend. So it sits between client and backend, somehow acting as a proxy.

Internally, it has a thread pool implemented for multithreading. The thread pool is a very naive one; it is a linked list of nodes waiting for a task.
When there is a task to be assigned, the acceptor thread will check availability in the thread pool, and if there is any, the acceptor thread will wake up the first available thread.
After the thread finished handling the request, it will queue itself to the end of the linked list.

## More about thread pool
There are 2 types of threads that are related to thread pool. First kind is the `worker thread`. `Worker threads` do the actual heavy lifting of handling request. They wait for the acceptor thread to accept a connection and assign the request to the worker threads.
Then there is the `acceptor thread`. The `acceptor thread` is dedicated to only accepting incoming connections from client.

## Backend connection pooling
The thread pool queues worker threads back to the linked list without closing the ESTABLISHED connection. If the transaction is complete and the connection is alive, we want to reuse that connection to avoid the cost of recurring TLS handshake. Thus, ESTABLISHED connections must be queued to the linked list of threads (pool) without closing the connection. 

# Building from Source Code
Go to the top directory and run following commands.

```bash
git clone git@github.com:YoloZoloo/varnish-encrypto.git
cd varnish-encrypto
cmake -B build
cmake --build build
```

# Executing the binary
From the top directory run 
```bash
build/varnish-encrypto $LISTENING_PORT $BACKEND_IP_OR_DOMAIN $BACKEND_SERVER_PORT
```

## Example:
Run `build/varnish-encrypto 8080 github.com 443`

Then, in a different terminal window:

`curl http://localhost:8080/YoloZoloo/varnish-encrypto -v -H "Host: github.com"`

