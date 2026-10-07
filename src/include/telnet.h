#ifndef TELNET_H
#define TELNET_H

// open "telnet" with an accepted tcpconn descriptor to read and write plain
// data over it, and closing it closes the tcpconn too

// control ops take the option as arg, and an enable takes effect once the client agrees
#define TELNET_LOCAL_ENABLED 1
#define TELNET_REMOTE_ENABLED 2
#define TELNET_LOCAL_ENABLE 3
#define TELNET_REMOTE_ENABLE 4

#define TELNET_OPTION_ECHO 1
#define TELNET_OPTION_SUPPRESS_GO_AHEAD 3

void telnet_init(void);

#endif
