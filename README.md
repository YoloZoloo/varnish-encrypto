# varnish-encrypto
The first intention of writing this piece of code was to implement backend ssl encryption for open source varnish, since Varnish Cache lacks this feature.

Now it is more of just a personal project. 

There is CMakeList.txt file for Macos and Linux each. You should be able to compile the source code using it, with caveat. Adjust included library path according to the openssl library in your settings. 

What does this code do?
It will receive unencrypted HTTP request from a client and encrypt it then send that request to the backend.
Internally, it has thread pool implemented to use multithreading. The thread pool is very naive one, it is just a linked list of nodes waiting for task.
When there is a task to be assigned, main thread will check availability in the thread pool and if there is any, main thread will wake up a first available thread for backend TLS handshake task. 
After thread is done with handling the request, it will queue itself to the end of the linked list. And only one mutex lock is used to avoid racing during queueing.
