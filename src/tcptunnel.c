/*
 * Rewritten by openlab.vn.ua project
 * Based on original work by Clemens Fuchslocher
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 *
 */

#if defined(__MINGW32__) || defined(__MINGW64__)
#define  FLAG_INIT_WINSOCK
#define  FLAG_DISABLE_FORK
#endif

#include <errno.h>
#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#ifdef FLAG_INIT_WINSOCK
#include <winsock2.h>
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#endif
#include <pthread.h>

#include "tcptunnel.h"

struct struct_rc rc;
struct struct_options options;

static struct option long_options[] = {
	{ "local-port",     required_argument, NULL, LOCAL_PORT_OPTION },
	{ "remote-host",    required_argument, NULL, REMOTE_HOST_OPTION },
	{ "remote-port",    required_argument, NULL, REMOTE_PORT_OPTION },
	{ "bind-address",   required_argument, NULL, BIND_ADDRESS_OPTION },
	{ "client-address", required_argument, NULL, CLIENT_ADDRESS_OPTION },
	{ "buffer-size",    required_argument, NULL, BUFFER_SIZE_OPTION },
	#ifndef FLAG_DISABLE_FORK
	{ "fork",           no_argument,       NULL, FORK_OPTION },
	#endif
	{ "log",            no_argument,       NULL, LOG_OPTION },
	{ "stay-alive",     no_argument,       NULL, STAY_ALIVE_OPTION },
	{ "help",           no_argument,       NULL, HELP_OPTION },
	{ "version",        no_argument,       NULL, VERSION_OPTION },
	{ "log-level",      required_argument, NULL, LOG_LEVEL_OPTION },
	{ "concurrency",    required_argument, NULL, CONCURRENCY_OPTION },
	{ "pipe-timeout",   required_argument, NULL, PIPE_TIMEOUT_OPTION },
	{ "log-data",       required_argument, NULL, LOG_DATA_MODE_OPTION },
	{ 0, 0, 0, 0 }
};

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

int main(int argc, char *argv[])
{
	#ifdef FLAG_INIT_WINSOCK
	WSADATA info;
	if (WSAStartup(MAKEWORD(1,1), &info) != 0)
	{
		perror("main: WSAStartup()");
		return 1;
	}
	#endif

	int opt_parse_result = set_options(argc, argv);

	if (opt_parse_result < 0)
	{
		return 1; // fail
	}
	else if (opt_parse_result > 0)
	{
		return 0; // done
	}

	// pre-resolve remote host, so it would not be resolved during runtime

	timestamp_buffer_t tsb;

	if ((options.log_level >= LOG_LEVEL_FULL))
	{
		printf("> %s tcptunnel: init: buffer-size=%d\n", get_current_timestamp(&tsb), options.buffer_size);
	}

	if ((options.log_level >= LOG_LEVEL_FULL) && (options.pipe_timeout > 0))
	{
		printf("> %s tcptunnel: init: pipe-timeout=%d (sec)\n", get_current_timestamp(&tsb), options.pipe_timeout);
	}

	if ((options.log_level >= LOG_LEVEL_FULL))
	{
		const char *concurrency_text = "";
		if      (options.concurrency == CONCURRENCY_NONE)    { concurrency_text = "none";    }
		else if (options.concurrency == CONCURRENCY_FORK)    { concurrency_text = "fork";    }
		else if (options.concurrency == CONCURRENCY_THREADS) { concurrency_text = "threads"; }
		printf("> %s tcptunnel: init: concurrency=%s\n", get_current_timestamp(&tsb), concurrency_text);
	}

	if ((options.log_level >= LOG_LEVEL_FULL) && (options.log_data != LOG_DATA_NONE))
	{
		const char *log_data_text = "";
		if      (options.log_data == LOG_DATA_BIN) { log_data_text = "bin"; }
		else if (options.log_data == LOG_DATA_HEX) { log_data_text = "hex"; }
		printf("> %s tcptunnel: init: log-data=%s\n", get_current_timestamp(&tsb), log_data_text);
	}

	if ((options.log_level >= LOG_LEVEL_FULL) && (options.concurrency == CONCURRENCY_NONE))
	{
		const char* stay_alive_text = (options.stay_alive ? "[active, will run continuously]" : "[not active, will end after 1 request]");
		printf("> %s tcptunnel: init: stay-alive %s\n", get_current_timestamp(&tsb), stay_alive_text);
	}

	if ((options.log_level >= LOG_LEVEL_FULL) && (options.client_address != NULL))
	{
		printf("> %s tcptunnel: init: allowed only client-address=[%s]\n", get_current_timestamp(&tsb), options.client_address);
	}

	if (options.log_level >= LOG_LEVEL_FULL)
	{
		printf("> %s tcptunnel: init: resolving remote-host=[%s]\n", get_current_timestamp(&tsb), options.remote_host);
	}

	rc.remote_host = gethostbyname(options.remote_host);
	if (rc.remote_host == NULL)
	{
		perror("main: gethostbyname()");
		return 1;
	}

	int server_socket = build_server();
	if (server_socket < 0)
	{
		return 1;
	}

	#ifndef FLAG_DISABLE_FORK
	signal(SIGCHLD, SIG_IGN);
	#endif

	do
	{
		int client_socket;
		client_socket = wait_for_clients(server_socket);
		if (client_socket >= 0)
		{
			handle_client(client_socket, server_socket);
		}
	}
	while (options.stay_alive);

	close(server_socket);

	return 0;
}

int set_options(int argc, char *argv[])
{
	// parses command line options, return 0 on success, negative on failure, positive on done-and-exit
	int opt;
	int index;

	const char* name = argv[0];

	if (argc <= 1)
	{
		print_usage(name);
		print_helpinfo(name);
		return 1;
	}

	memset(&options, 0, sizeof(options));

	options.buffer_size = BUFFER_SIZE_DEFAULT;

	do
	{
		opt = getopt_long(argc, argv, "", long_options, &index);
		switch (opt)
		{
			case LOCAL_PORT_OPTION:
			{
				options.local_port = optarg == NULL ? 0 : atoi(optarg);
				break;
			}

			case REMOTE_PORT_OPTION:
			{
				options.remote_port = optarg == NULL ? 0 : atoi(optarg);
				break;
			}

			case REMOTE_HOST_OPTION:
			{
				options.remote_host = optarg;
				break;
			}

			case BIND_ADDRESS_OPTION:
			{
				options.bind_address = optarg;
				break;
			}

			case BUFFER_SIZE_OPTION:
			{
				options.buffer_size = optarg == NULL ? 0 : atoi(optarg);
				break;
			}

			case CLIENT_ADDRESS_OPTION:
			{
				options.client_address = optarg;
				break;
			}

			case FORK_OPTION:
			{
				options.concurrency = CONCURRENCY_FORK;
				break;
			}

			case LOG_OPTION:
			{
				options.log_level = LOG_LEVEL_FULL;
				options.log_data = LOG_DATA_BIN;
				break;
			}

			case STAY_ALIVE_OPTION:
			{
				options.stay_alive = TRUE;
				#if defined(FLAG_DISABLE_FORK)
				options.concurrency = CONCURRENCY_THREADS;
				#elif defined(__CYGWIN__)
				options.concurrency = CONCURRENCY_THREADS; // preffer this on windows
				#else
				options.concurrency = CONCURRENCY_FORK;
				#endif
				break;
			}

			case LOG_LEVEL_OPTION:
			{
				options.log_level = optarg == NULL ? 0 : atoi(optarg);
				break;
			}

			case CONCURRENCY_OPTION:
			{
				if (optarg == NULL)
				{
					options.concurrency = CONCURRENCY_THREADS;
				}
				else if (strcmp(optarg, "fork") == 0)
				{
					#ifdef FLAG_DISABLE_FORK
					options.concurrency = CONCURRENCY_THREADS;
					#else
					options.concurrency = CONCURRENCY_FORK;
					#endif
				}
				else if ((strcmp(optarg, "pthread") == 0) || (strcmp(optarg, "thread") == 0))
				{
					options.concurrency = CONCURRENCY_THREADS;
				}
				else if ((strcmp(optarg, "pthreads") == 0) || (strcmp(optarg, "threads") == 0))
				{
					options.concurrency = CONCURRENCY_THREADS;
				}
				else if ((strcmp(optarg, "none") == 0) || (strcmp(optarg, "single") == 0))
				{
					options.concurrency = CONCURRENCY_NONE;
				}
				else
				{
					print_missing(name, "invalid '--concurrency=' option.");
					return -1;
				}
				break;
			}

			case PIPE_TIMEOUT_OPTION:
			{
				options.pipe_timeout = optarg == NULL ? 0 : atoi(optarg);
				break;
			}

			case LOG_DATA_MODE_OPTION:
			{
				if (optarg == NULL)
				{
					options.log_data = LOG_DATA_BIN;
				}
				else if (strcmp(optarg, "hex") == 0)
				{
					options.log_data = LOG_DATA_HEX;
				}
				else if (strcmp(optarg, "bin") == 0)
				{
					options.log_data = LOG_DATA_BIN;
				}
				else if (strcmp(optarg, "none") == 0)
				{
					options.log_data = LOG_DATA_NONE;
				}
				else
				{
					print_missing(name, "invalid '--log-data=' option.");
					return -1;
				}
				break;
			} 

			case HELP_OPTION:
			{
				print_usage(name);
				print_help(name);
				print_examples(name);
				return 1;
			}

			case VERSION_OPTION:
			{
				print_version(name);
				return 1;
			}

			case '?':
			{
				print_usage(name);
				print_helpinfo(name);
				return 1;
			}
		}
	}
	while (opt != -1);

	if (options.buffer_size <= 0)
	{
		print_missing(name, "invalid '--buffer-size=' option.");
		return -1;
	}

	if (options.local_port <= 0)
	{
		print_missing(name, "missing '--local-port=' option.");
		return -1;
	}

	if (options.remote_port <= 0)
	{
		print_missing(name, "missing '--remote-port=' option.");
		return -1;
	}

	if (options.remote_host == NULL)
	{
		print_missing(name, "missing '--remote-host=' option.");
		return -1;
	}

	if (options.concurrency != CONCURRENCY_NONE)
	{
		options.stay_alive = TRUE;
	}

	return 0;
}

// mt safe in_addr (IPv4) addr printing
#ifdef FLAG_INET_NTOA_MT_SAFE
// use inet_ntoa to print address as we assume it mt-safe (true for glibc)
//#define inet_ntoa_rbuf(in,buffer,bufflen) inet_ntoa(in)
static char* inet_ntoa_rbuf(struct in_addr in, char* buffer, int bufflen)
{
	return inet_ntoa(in);
}
#else
// use local implemenation of inet_ntoa_r, as inet_ntoa_r my not be available
static char* inet_ntoa_rbuf(struct in_addr in, char* buffer, int bufflen)
{
	if (bufflen > 0)
	{
		unsigned char* bytes = (unsigned char*)&in;
		int result = snprintf(buffer, bufflen, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3]);
		if (result >= bufflen)
		{
			buffer[bufflen-1] = 0; // Enforce results as null-terminated
		}
	}

	return buffer;
}

#endif

int build_server(void)
{
	// return server_socket or negative value on error
	timestamp_buffer_t tsb;
	inet_a_buffers_t inetabs;
	struct sockaddr_in server_addr;

	memset(&server_addr, 0, sizeof(server_addr));

	server_addr.sin_port = htons(options.local_port);
	server_addr.sin_family = AF_INET;
	server_addr.sin_addr.s_addr = INADDR_ANY;

	int server_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (server_socket < 0)
	{
		perror("build_server: socket()");
		return -1;
	}
	
	int optval = 1;
	#if (defined(__MINGW32__) || defined(__MINGW64__))
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, (const char *) &optval, sizeof(optval)) < 0)
	#else
	if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) < 0)
	#endif
	{
		perror("build_server: setsockopt(SO_REUSEADDR)");
		return -1;
	}

	if (options.bind_address != NULL)
	{
		server_addr.sin_addr.s_addr = inet_addr(options.bind_address);
	}

	if (bind(server_socket, (struct sockaddr *) &server_addr, sizeof(server_addr)) < 0)
	{
		perror("build_server: bind()");
		return -1;
	}

	if (listen(server_socket, 1) < 0)
	{
		perror("build_server: listen()");
		return -1;
	}

	if (options.log_level >= LOG_LEVEL_FULL)
	{
		printf("> %s tcptunnel: listening at %s:%d\n", get_current_timestamp(&tsb), inet_ntoa_rbuf(server_addr.sin_addr, inetabs, sizeof(inetabs)), options.local_port);
	}

	return server_socket;
}

int wait_for_clients(int server_socket)
{
	// returns client_socket or negative value on error
	timestamp_buffer_t tsb;
	inet_a_buffers_t inetabs;
	#if (defined(__MINGW32__) || defined(__MINGW64__) || defined(__CYGWIN__))
	int client_addr_size;
	#else
	unsigned int client_addr_size;
	#endif

	struct sockaddr_in client_addr;
	client_addr_size = sizeof(client_addr);

	int client_socket = accept(server_socket, (struct sockaddr *) &client_addr, &client_addr_size);
	if (client_socket < 0)
	{
		if (errno != EINTR)
		{
			perror("wait_for_clients: accept()");
		}
		return -1;
	}

	if ((options.client_address != NULL) && (strcmp(inet_ntoa_rbuf(client_addr.sin_addr, inetabs, sizeof(inetabs)), options.client_address) != 0))
	{
		if (options.log_level >= LOG_LEVEL_BRIEF)
		{
			printf("> %s tcptunnel: refused request from client %s\n", get_current_timestamp(&tsb), inet_ntoa_rbuf(client_addr.sin_addr, inetabs, sizeof(inetabs)));
		}
		close(client_socket);
		return -1;
	}

	if (options.log_level >= LOG_LEVEL_BRIEF)
	{
		printf("> %s tcptunnel: [%d+?] accepted request from client %s\n", get_current_timestamp(&tsb), client_socket, inet_ntoa_rbuf(client_addr.sin_addr, inetabs, sizeof(inetabs)));
	}

	return client_socket;
}

// pass int parameter as void * we threat void * as int here // usefull to pthread_create
// (it is not fair play, but works, as intptr_t at least size of int in all imaginable cases)
#define INT_AS_VOID_PTR(i) ((void *)((intptr_t)(i)))
#define VOID_PTR_AS_INT(p) ((int)((intptr_t)(p)))

static void *handle_tunnel_mt(void *iclient_socket)
{
	static long thread_num = 0;

	timestamp_buffer_t tsb;

	int client_socket = VOID_PTR_AS_INT(iclient_socket);

	long thread_id = ++thread_num;

	if (options.log_level >= LOG_LEVEL_FULL)
	{
		printf("> %s tcptunnel: [%d+?] thread %ld started\n", get_current_timestamp(&tsb), client_socket, (long)thread_id);
	}

	handle_tunnel(client_socket);

	if (options.log_level >= LOG_LEVEL_FULL)
	{
		printf("> %s tcptunnel: [%d+?] thread %ld ended\n", get_current_timestamp(&tsb), client_socket, (long)thread_id);
	}

	return NULL;
}

void handle_client(int client_socket, int server_socket)
{
	if (options.concurrency == CONCURRENCY_NONE)
	{
		handle_tunnel(client_socket);
	}
	#ifndef FLAG_DISABLE_FORK
	else if (options.concurrency == CONCURRENCY_FORK)
	{
		if (fork() == 0)
		{
			// child
			timestamp_buffer_t tsb;

			close(server_socket);

			long proccess_id = getpid();

			if (options.log_level >= LOG_LEVEL_FULL)
			{
				printf("> %s tcptunnel: [%d+?] proccess %ld started\n", get_current_timestamp(&tsb), client_socket, (long)proccess_id);
			}

			handle_tunnel(client_socket);

			if (options.log_level >= LOG_LEVEL_FULL)
			{
				printf("> %s tcptunnel: [%d+?] proccess %ld ended\n", get_current_timestamp(&tsb), client_socket, (long)proccess_id);
			}

			exit(0);
		}
		else
		{
			// parent
			close(client_socket);
		}
	}
	#endif
	else
	{
		// mutithread
		pthread_t proc_thread;
		if (pthread_create(&proc_thread, NULL, &handle_tunnel_mt, INT_AS_VOID_PTR(client_socket)))
		{
			perror("handle_client: pthread_create");
			return;
		}
		return;
	}
}

void handle_tunnel(int client_socket)
{
	int remote_socket;
	remote_socket = build_tunnel(client_socket);
	if (remote_socket >= 0)
	{
		use_tunnel(client_socket, remote_socket);
	}
	else
	{
		close(client_socket);
	}
}

int build_tunnel(int client_socket)
{
	// returns remote_socket or negative value on error
	timestamp_buffer_t tsb;
	inet_a_buffers_t inetabs;

	struct sockaddr_in remote_addr;

	memset(&remote_addr, 0, sizeof(remote_addr));

	remote_addr.sin_family = AF_INET;
	remote_addr.sin_port = htons(options.remote_port);

	memcpy(&remote_addr.sin_addr.s_addr, rc.remote_host->h_addr, rc.remote_host->h_length);

	int remote_socket = socket(AF_INET, SOCK_STREAM, 0);
	if (remote_socket < 0)
	{
		perror("build_tunnel: socket()");
		return -1;
	}

	if (options.log_level >= LOG_LEVEL_FULL)
	{
		printf("> %s tcptunnel: [%d+?] connecting to remote %s:%d\n", get_current_timestamp(&tsb), client_socket, inet_ntoa_rbuf(remote_addr.sin_addr, inetabs, sizeof(inetabs)), options.remote_port);
	}

	if (connect(remote_socket, (struct sockaddr *) &remote_addr, sizeof(remote_addr)) < 0)
	{
		if (options.log_level >= LOG_LEVEL_BRIEF)
		{
			printf("> %s tcptunnel: [%d+?] failed to connect to remote %s:%d\n", get_current_timestamp(&tsb), client_socket, inet_ntoa_rbuf(remote_addr.sin_addr, inetabs, sizeof(inetabs)), options.remote_port);
		}

		perror("build_tunnel: connect()");
		return -1;
	}

	if (options.log_level >= LOG_LEVEL_FULL)
	{
		printf("> %s tcptunnel: [%d+%d] pipe connected to remote %s:%d\n", get_current_timestamp(&tsb), client_socket, remote_socket, inet_ntoa_rbuf(remote_addr.sin_addr, inetabs, sizeof(inetabs)), options.remote_port);
	}

	return remote_socket;
}

static void print_dump_hex(void* buffer, size_t size)
{
	// for debug only: out hi loaded systems this may be interleaved with data from different threads
	size_t offset;

	size_t hex_line_size = 32;

	for (offset = 0; offset < size; offset++)
	{
		unsigned int data = ((const unsigned char*)buffer)[offset];
		printf("%02X", data);
		if (hex_line_size > 0)
		{
			if ((((offset + 1) % hex_line_size) == 0) && ((offset + 1) < size))
			{
				printf("\n");
			}
		}
	}
}

int use_tunnel(int client_socket, int remote_socket)
{
	// return 0 on success or negative value on error
	fd_set io;
	timestamp_buffer_t tsb;
	char buffer[options.buffer_size];

	int result = 0;

	for (;;)
	{
		FD_ZERO(&io);
		FD_SET(client_socket, &io);
		FD_SET(remote_socket, &io);

		struct timeval timeout;
		memset(&timeout, 0, sizeof(timeout));
		timeout.tv_sec = options.pipe_timeout;

		if (options.log_level >= LOG_LEVEL_FULL)
		{
			//printf("> %s tcptunnel: [%d+%d] pipe is waiting\n", get_current_timestamp(&tsb), client_socket, remote_socket);
		}

		int select_result = select(fd(client_socket, remote_socket), &io, NULL, NULL, (options.pipe_timeout <= 0 ? NULL : &timeout));

		if (select_result < 0)
		{
			perror("use_tunnel: select()");
			result = -1;
			break;
		}
		else if (select_result == 0)
		{
			printf("> %s tcptunnel: [%d+%d] pipe timeout of %d sec elapsed\n", get_current_timestamp(&tsb), client_socket, remote_socket, options.pipe_timeout);
			result = -1;
			break;
		}

		if (FD_ISSET(client_socket, &io))
		{
			int count = recv(client_socket, buffer, sizeof(buffer), 0);

			if (count < 0)
			{
				perror("use_tunnel: recv(client_socket)");
				result = -1;
				break;
			}

			if (count == 0)
			{
				if (options.log_level >= LOG_LEVEL_FULL)
				{
					printf("> %s tcptunnel: [%d+%d] pipe closed by client\n", get_current_timestamp(&tsb), client_socket, remote_socket);
				}

				result = 0;
				break;
			}

			if (options.log_data > LOG_DATA_NONE)
			{
				printf("> %s tcptunnel: [%d+%d] pipe received %d bytes from client [dump:start]\n", get_current_timestamp(&tsb), client_socket, remote_socket, count);
				if (options.log_data == LOG_DATA_HEX)
				{
					print_dump_hex(buffer, count);
				}
				else
				{
					fwrite(buffer, sizeof(char), count, stdout);
					//fflush(stdout);
				}
				printf("\n");
				printf("> %s tcptunnel: [%d+%d] pipe received %d bytes from client [dump:end]:\n", get_current_timestamp(&tsb), client_socket, remote_socket, count);
			}
			else if (options.log_level >= LOG_LEVEL_FULL)
			{
				printf("> %s tcptunnel: [%d+%d] pipe received %d bytes from client\n", get_current_timestamp(&tsb), client_socket, remote_socket, count);
			}

			if (send(remote_socket, buffer, count, 0) <= 0)
			{
				perror("use_tunnel: send(remote_socket)");
				result = -1;
				break;
			}
		}

		if (FD_ISSET(remote_socket, &io))
		{
			int count = recv(remote_socket, buffer, sizeof(buffer), 0);
			if (count < 0)
			{
				perror("use_tunnel: recv(remote_socket)");
				result = -1;
				break;
			}

			if (count == 0)
			{
				if (options.log_level >= LOG_LEVEL_FULL)
				{
					printf("> %s tcptunnel: [%d+%d] pipe closed by remote\n", get_current_timestamp(&tsb), client_socket, remote_socket);
				}

				result = 0;
				break;
			}

			if (options.log_data > LOG_DATA_NONE)
			{
				printf("> %s tcptunnel: [%d+%d] pipe received %d bytes from remote [dump:start]\n", get_current_timestamp(&tsb), client_socket, remote_socket, count);
				if (options.log_data == LOG_DATA_HEX)
				{
					print_dump_hex(buffer, count);
				}
				else
				{
					fwrite(buffer, sizeof(char), count, stdout);
					//fflush(stdout);
				}
				printf("\n");
				printf("> %s tcptunnel: [%d+%d] pipe received %d bytes from remote [dump:end]\n", get_current_timestamp(&tsb), client_socket, remote_socket, count);
			}
			else if (options.log_level >= LOG_LEVEL_FULL)
			{
				printf("> %s tcptunnel: [%d+%d] pipe received %d bytes from remote\n", get_current_timestamp(&tsb), client_socket, remote_socket, count);
			}

			if (send(client_socket, buffer, count, 0) <= 0)
			{
				perror("use_tunnel: send(client_socket)");
				result = -1;
				break;
			}
		}
	}

	close(client_socket);
	close(remote_socket);
	return result;
}

int fd(int client_socket, int remote_socket)
{
	unsigned int fd = client_socket;
	if (fd < remote_socket)
	{
		fd = remote_socket;
	}
	return fd + 1;
}

char *get_current_timestamp_r(char *timestamp_buffer, size_t timestamp_buffer_size)
{
	time_t date;
	time(&date);
	strftime(timestamp_buffer, timestamp_buffer_size, "%Y-%m-%d %H:%M:%S", localtime(&date));
	return timestamp_buffer;
}

char* get_current_timestamp(timestamp_buffer_t* buffer)
{
	return get_current_timestamp_r(buffer->buffer, sizeof(buffer->buffer));
}

void print_usage(const char* name)
{
	fprintf(stderr, "TCP/IP connection tunneling utility\n");
	fprintf(stderr, "Usage: %s [options]\n\n", name);
}

void print_helpinfo(const char* name)
{
	fprintf(stderr, "Try `%s --help' for more options\n", name);
}

void print_help(const char* name)
{
	fprintf(stderr, "\
Options:\n\
  --version\n\
  --help\n\n\
  --local-port=PORT    local port to listen at   (required)\n\
  --remote-port=PORT   remote port to connect to (required)\n\
  --remote-host=HOST   remote host to connect to (required)\n\
  --bind-address=IP    bind address to listen at (optional)\n\
  --client-address=IP  only accept connections from this IP  (work as filter)\n\
  --buffer-size=BYTES  buffer size [default " MACRO_STR(BUFFER_SIZE_DEFAULT_IN_KB) "K]\n"
	#ifndef FLAG_DISABLE_FORK
	"\
  --fork               fork-based concurrency    (same as --concurrency=fork)\n"
	#endif
	"\
  --log                turns on log    (same as --log-level=1 --log-data=bin)\n\
  --stay-alive         stay alive after first request  (turns on concurrency)\n\
  --log-level=LEVEL    logging: 0=off,1=brief,2=full              [default 0]\n\
  --concurrency=MODEL  concurrency model: none,fork,threads    [default none]\n\
  --pipe-timeout=N     pipe data transfer timeout in sec (0=none) [default 0]\n\
  --log-data=MODE      dump pipe data mode: none,hex,bin       [default:none]\n\
\n");
}

void print_examples(const char* name)
{
	fprintf(stderr, "\
Example:\n\
tcptunnel --remote-port=80 --local-port=9980 --remote-host=acme.com --stay-alive --log-level=2\n\
");
}

void print_version(const char* name)
{
	fprintf(stderr, "\
tcptunnel v" VERSION " Rewritten by openlab.vn.ua\n\n\
This program is distributed in the hope that it will be useful,\n\
but WITHOUT ANY WARRANTY; without even the implied warranty of\n\
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n\
GNU General Public License for more details.\n\n\
Based on implementation written by Clemens Fuchslocher\n\
");
}

void print_missing(const char* name, const char *message)
{
	print_usage(name);
	fprintf(stderr, "%s: %s\n", name, message);
	print_helpinfo(name);
}

