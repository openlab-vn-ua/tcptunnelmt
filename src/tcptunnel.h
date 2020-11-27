#ifndef TCPTUNNEL_H
#define TCPTUNNEL_H

#define VERSION "0.9"

#define LOCAL_PORT_OPTION     'a'
#define REMOTE_PORT_OPTION    'b'
#define REMOTE_HOST_OPTION    'c'
#define BIND_ADDRESS_OPTION   'd'
#define CLIENT_ADDRESS_OPTION 'e'
#define BUFFER_SIZE_OPTION    'f'
#define FORK_OPTION           'g'
#define LOG_OPTION            'h'
#define STAY_ALIVE_OPTION     'i'
#define HELP_OPTION           'j'
#define VERSION_OPTION        'k'
#define LOG_LEVEL_OPTION      'l'
#define CONCURRENCY_OPTION    'm'
#define PIPE_TIMEOUT_OPTION   'n'
#define LOG_DATA_MODE_OPTION  'o'

int build_server(void);
int wait_for_clients(int server_socket);
void handle_client(int client_socket, int server_socket);
void handle_tunnel(int client_socket);
int build_tunnel(int client_socket);
int use_tunnel(int client_socket, int remote_socket);
int fd(int client_socket, int remote_socket);

int set_options(int argc, char *argv[]);

char* get_current_timestamp_r(char* timestamp_buffer, size_t timestamp_buffer_size);

#define TIMESTAMP_BUFFER_SIZE 20
typedef struct { char buffer[TIMESTAMP_BUFFER_SIZE]; } timestamp_buffer_t;
char* get_current_timestamp(timestamp_buffer_t* buffer);

void print_help(const char *name);
void print_helpinfo(const char* name);
void print_usage(const char* name);
void print_examples(const char* name);
void print_version(const char* name);
void print_missing(const char* name, const char *message);

#define MACRO_STR_INDIR(x) #x // internal use only
#define MACRO_STR(x) MACRO_STR_INDIR(x) // converts x argument to "x" string representation

struct struct_options {
	int         local_port;
	const char *remote_host;
	int         remote_port;
	const char *bind_address;
	const char *client_address;
	int         buffer_size;
	int         log_level;
	int         pipe_timeout;
	int         concurrency;
	int         stay_alive;
	int         log_data;
};

#define LOG_DATA_NONE 0
#define LOG_DATA_HEX  1
#define LOG_DATA_BIN  2

#define BUFFER_SIZE_DEFAULT_IN_KB 32
#define BUFFER_SIZE_DEFAULT (BUFFER_SIZE_DEFAULT_IN_KB*1024)

#define LOG_LEVEL_NONE     0 // default
#define LOG_LEVEL_BRIEF    1
#define LOG_LEVEL_FULL     2

#define CONCURRENCY_NONE    0 // default
#define CONCURRENCY_THREADS 1
#define CONCURRENCY_FORK    2

struct struct_rc {
	struct hostent *remote_host;
};

// buffer for inet address text
#define INET_ADDR_TEXT_MAX_SIZE   (39+1) // IPv6 string is 39 char max, IPv4 string is 15 char max 
typedef char inet_a_buffers_t[INET_ADDR_TEXT_MAX_SIZE+1];

#endif

