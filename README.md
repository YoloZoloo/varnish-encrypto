# varnish-encrypto

The initial intention of writing this piece of code was to implement backend tls encryption for open source varnish, since Varnish Cache lacks this feature.

Now it is more of just a personal project.

There is cmake file so that you should be able to compile the source code using it, with caveat that necessary library paths should be adjusted according to the openssl, pthread library in your settings.

What does this code do?
It will receive unencrypted HTTP request from a client then encrypt it and send that request to the backend.
Internally, it has a thread pool implemented for multithreading. The thread pool is very naive one, it is a linked list of nodes waiting for task.
When there is a task to be assigned, acceptor thread will check availability in the thread pool and if there is any, acceptor thread will wake up a first available thread.
After thread is done handling the request, it will queue itself to the end of the linked list. And only one mutex lock is used to avoid racing during queueing.

# Building from Source Code

Go to the top directory and run following commands.

```bash
cmake -B build
cmake --build build
```
