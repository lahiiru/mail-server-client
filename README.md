# Simple mail server and client
File based mail server and client for demonstration sockets and multithreading. Written in C using pthreads.

# Server

Compile server.c
`gcc server.c -lpthread -o server`

Start server by specifying number of threads and listening port
`./server [threads] [port]`

# Client

Compile client.c
`gcc client.c -lpthread -o client`

Start client by specifiying server's IP and server's listening port.
`./client [ip] [port]`

Now start using the mail server!

# Commands

`make <client_name>` - Registers new client and make mail box in the server.
`get_client_list` - Get list of registered clients.
`send <receiver_name>` - Start composing a mail to another registered user. Follow the instructions given in comman line.
`get_mailbox <client_name>` - Get mail box information of a registered user
`read <client_name mail_id>` - Read a mail specified by mail ID
`delete <client_name mail_id>` - Delete a mail specified by mail ID
`quit` - Exit
