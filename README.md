For CSC524: Computer Networks, Spring 2014, University of Miami

Client implementation: Joseph Choi

Server implementation: Oliver Chang

NAME

    mytftp

SYNOPSIS

    mytftp -l [-p port] [-v]
    mytftp [-p port] [-v] -[r|w] file host


DESCRIPTION

    Implements a client and a server for the tftp protocol. The first form initiates a
    server. The second form intiates a client to read or write file "file" from host
    "host".


OPTIONS

    -l mytftp should go into server mode
    -p as server, mytftp should listen on port port; as client, mytftp should make
       its connection to port port.
    -v verbose. helpful debugging output to stdout.
    -r client should copy the file on host to local, both files havving the name "file".
    -w client should copy the local file to host, both files having the name "file".

NOTES

    To avoid any misuse, obey the usual safety restrictions: only read or write to a single
    directory. Only read or write existing files with everyone privileges.

    If -p is not specfied, the default port is 3335.

    Implement only octet. Forget netascii and email modes. If requested return an error
    packet with message "mode not supported".

    Save timeout retransmissions for last. Get it working in the optimistic case of no lost
    packets. How are you going to simulate lost packets so that you can test
    your retransmission code?

LAST UPDATED

    February 13, 2013


