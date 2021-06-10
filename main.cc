/*
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * @author: pregusia
 *
 */


#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <getopt.h>
#include <signal.h>
#include <unistd.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/wait.h>

#include <fcntl.h>
#include <dirent.h>
#include <unistd.h>
#include <netdb.h>

#include <arpa/inet.h>

volatile int g_running;
volatile int g_quiet;
volatile int g_abort;

sockaddr_in net_resolve_addr(char* host, int port) {
    sockaddr_in res = { 0 };
    res.sin_family = AF_INET;
    res.sin_port = htons(port);

    if (inet_addr(host) != INADDR_NONE) {
        res.sin_addr.s_addr = inet_addr(host);
        return res;
    }

    hostent* he = gethostbyname(host);
    if (he != NULL) {
        in_addr** addr_list = (in_addr**)he->h_addr_list;
        if (addr_list[0] != NULL) {
            res.sin_addr = *addr_list[0];
            return res;
        }
    }

    res.sin_addr.s_addr = INADDR_NONE;
    return res;
}

int net_connect(char* host, int port) {
    int fd = 0;
    int res = 0;
    struct sockaddr_in addr;

    addr = net_resolve_addr(host, port);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        if (!g_quiet) fprintf(stderr,"ERROR: Cannot resolve host\n");
        exit(1);
    }

    fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) {
        perror("socket");
        exit(1);
    }

    res = connect(fd, (struct sockaddr*)&addr, sizeof(sockaddr_in));
    if (res < 0) {
        close(fd);
        return -1;
    }

    return fd;
}

int net_read_fully(int fd, char* buf, int bufSize, int timeLimit) {
    fd_set set;
    int res = 0;
    struct timeval tv;
    time_t startTime;
    int pos = 0;

    while(g_running) {
        FD_ZERO(&set);
        FD_SET(fd, &set);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        res = select(fd + 1, &set, NULL, NULL, &tv);
        if (res == 1) {
            res = recv(fd, buf + pos, bufSize - pos, 0);
            if (res == 0) return pos;
            if (res < 0) return -1;
            pos += res;
            if (pos >= bufSize) return bufSize;
        }
        if (res < 0) {
            return -1;
        }
    }

    return -1;
}

int start_server(char* host, int port, char* text) {
    fd_set set;
    int server_fd = 0;
    int client_fd = 0;
    int en = 1;
    int res = 0;
    struct sockaddr_in addr;
    struct timeval tv;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
      perror("socket");
      return 1;
    }

    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &en, sizeof(en));

    addr = net_resolve_addr(host, port);
    if (addr.sin_addr.s_addr == INADDR_NONE) {
        close(server_fd);
        if (!g_quiet) fprintf(stderr,"ERROR: Cannot resolve host\n");
        return 1;
    }

    if (bind(server_fd,(sockaddr*) &addr,sizeof(sockaddr_in)) != 0) {
        close(server_fd);
        perror("bind");
        return 1;
    }

    if (listen(server_fd, 10) != 0) {
        close(server_fd);
        perror("listen");
        return 1;
    }

    g_running = 1;
    while(g_running) {
        FD_ZERO(&set);
        FD_SET(server_fd, &set);

        tv.tv_sec = 1;
        tv.tv_usec = 0;

        res = select(server_fd + 1, &set, NULL, NULL, &tv);
        if (res == 1) {
            int client_fd = accept(server_fd, NULL, NULL);
            int _rc=write(client_fd, text, strlen(text));
            close(client_fd);
        }

    }

    return g_abort;
}

int start_client(char* host, int port, char* text) {
    int client_fd = 0;
    int res = 0;
    char buf[1024];

    g_running = 1;
    while(g_running) {
        client_fd = net_connect(host, port);
        if (client_fd > 0) {

            memset(buf, 0, sizeof(buf));
            res = net_read_fully(client_fd, buf, sizeof(buf), 5);

            if (res > 0) {
                if (strcmp(buf, text) == 0) {
                    // ok
                    return 0;
                } else {
                    if (!g_quiet) fprintf(stderr,"WARN: response mismatch \"%s\" != \"%s\"\n", buf, text);
                }
            }
            close(client_fd);
        }
    sleep(1);
    }
    return g_abort;
}

void on_signal(int sig) {
    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    if (sig == SIGTERM || sig == SIGINT) {
        g_abort=1;
        g_running = 0;
    }
}

void print_usage(char* appName) {
    fprintf(stderr,"%s [-v] <-s [-h 0.0.0.0] | -c -h host> -p PORT [TEXT|\"READY\"]\n", appName);
}

int main(int argc, char** argv) {
    char conf_text[1024];
    char conf_host[1024];
    int conf_port;
    int mode_server = 0;
    int mode_client = 0;
    char const *default_host = "0.0.0.0";
    char const *default_text = "READY";
    int const default_port = 9999;

    g_running = 0;
    g_abort = 0;
    g_quiet = 1;
    conf_port = 0;
    memset(conf_text, 0, sizeof(conf_text));
    memset(conf_host, 0, sizeof(conf_host));

    while (1) {
        int c = getopt(argc, argv, "vcsp:h:");
        if (c == -1) break;
        switch (c) {
            case 'c':
                mode_client = 1;
                break;

            case 's':
                mode_server = 1;
                break;

            case 'p':
                conf_port = atoi(optarg);
                break;

            case 'h':
                strncpy(conf_host, optarg, sizeof(conf_host));
                break;
            case 'v':
                g_quiet = 0;
                break;
            case '?':
            default:
                print_usage(argv[0]);
                return 1;
        }
    }

    if (optind < argc) strncpy(conf_text, argv[optind], sizeof(conf_text));

    if (mode_server && mode_client) {
        if (!g_quiet) fprintf(stderr,"ERROR: The -s and -c are mutually exclusive\n");
        return 1;
    }

    if (!mode_server && !mode_client) {
    print_usage(argv[0]);
        return 1;
    }

    if (conf_port == 0) conf_port = default_port;

    if (conf_port <= 0 || conf_port > 65000) {
        if (!g_quiet) fprintf(stderr,"ERROR: Invalid port given\n");
        return 1;
    }

    if (conf_host[0] == 0) {
    if (mode_server) {
       strncpy(conf_host, default_host, strlen(default_host));
    } else {
            if (!g_quiet) fprintf(stderr,"ERROR: No host given\n");
            return 1;
    }
    }

    if (conf_text[0] == 0) {
    strncpy(conf_text,default_text,strlen(default_text));
    }

    signal(SIGTERM, on_signal);
    signal(SIGINT, on_signal);

    if (mode_server) {
        return start_server(conf_host, conf_port, conf_text);
    }
    if (mode_client) {
        return start_client(conf_host, conf_port, conf_text);
    }
    return 0;
}
