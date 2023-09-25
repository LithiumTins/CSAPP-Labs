#define _POSIX_C_SOURCE 200112L
#define _XOPEN_SOURCE 500

#include "csapp.h"
#include <unistd.h>

/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* You won't lose style points for including this long line in your code */
static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (X11; Linux x86_64; rv:10.0.3) Gecko/20120305 Firefox/10.0.3\r\n";

// my own things
#define BUF_SIZE MAX_OBJECT_SIZE
#define LINE_MAX 128
#define ENDING "\r\n"

struct cache_node
{
    int size;
    char *addr;
    char *file;
    char *target;
    struct cache_node *next;
};
struct cache_node header_node = { 0, NULL, NULL, NULL, NULL };
struct cache_node *header = &header_node;

pthread_mutex_t cache_mutex = PTHREAD_MUTEX_INITIALIZER;

void thread_close_fd(void *arg);
void thread_free_buf(void *arg);

void *thread_func(void *arg);

int main(int argc, char *argv[])
{
    // initialize server address
    struct sockaddr_in saddr;
    memset(&saddr, 0, sizeof(saddr));
    saddr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    saddr.sin_family = AF_INET;
    saddr.sin_port = argc > 1 ? htons(atoi(argv[1])) : htons(15214);

    // set the listening socket
    int sfd, opt_val = 1;
    sfd = Socket(AF_INET, SOCK_STREAM, 0);
    Setsockopt(sfd, SOL_SOCKET, SO_REUSEADDR, &opt_val, sizeof(opt_val));
    Bind(sfd, (struct sockaddr *)&saddr, sizeof(saddr));
    Listen(sfd, 10);

    // wait for client and serve then one after one
    int cfd = -1;
    while (1)
    {
        // wait until a client connect
        struct sockaddr_in caddr;
        socklen_t len = sizeof(caddr);
        if (cfd >= 0)
            Close(cfd);
        int cfd = Accept(sfd, (struct sockaddr *)&caddr, &len);
        // Write(STDOUT_FILENO, "wait\n", 5);
        pthread_attr_t attr;
        pthread_t tid;
        pthread_attr_init(&attr);
        pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
        Pthread_create(&tid, &attr, thread_func, (void *)(long)cfd);
    }

    printf("%s", user_agent_hdr);
    return 0;
}

void thread_close_fd(void *arg)
{
    int fd = (int)(long)arg;
    Close(fd);
}

void thread_free_buf(void *arg)
{
    Free(arg);
}

void *thread_func(void *arg)
{
    pthread_cleanup_push(thread_close_fd, arg);

    char *read_buf, *write_buf;
    read_buf = Malloc(BUF_SIZE);
    write_buf = Malloc(BUF_SIZE);

    int cfd = (int)(long)arg, read_total = 0;

    pthread_cleanup_push(thread_free_buf, write_buf);
    pthread_cleanup_push(thread_free_buf, read_buf);

    // read the request to read_buf
    int num = Read(cfd, read_buf, BUF_SIZE);
    read_buf[num] = '\0';
    char *header_end = strstr(read_buf, "\r\n\r\n");
    int header_end_posi = header_end - read_buf;
    if (!header_end)
    {
        Write(STDERR_FILENO, "\nProxy: Can't find end of headers, message is:\n", 47);
        Write(STDERR_FILENO, read_buf, num);
        Pthread_exit(NULL);
    }
    for (int i = 0; i < num; i++)
    {
        if (read_buf[i] == '\0' && i < header_end_posi)
        {
            Write(STDERR_FILENO, "\nProxy: Unexpected null charactor found, message is:\n", 53);
            Write(STDERR_FILENO, read_buf, num);
            Pthread_exit(NULL);
        }
    }
    // Write(STDOUT_FILENO, "read\n", 5);
    // Write(STDOUT_FILENO, read_buf, num);

    // divide http message into three parts
    char *request, *headers, *text, *part_end;
    request = read_buf;

    part_end = strstr(request, ENDING);
    if (!part_end)
    {
        Write(STDERR_FILENO, "\nProxy: Invalid message(\\r\\n not found for the request)\n", 56);
        Pthread_exit(NULL);
    }
    *part_end = '\0';
    headers = part_end + 2;

    part_end = strstr(headers, ENDING ENDING);
    if (!part_end)
    {
        Write(STDERR_FILENO, "\nProxy: invalid message(end of header not found)\n", 49);
        Pthread_exit(NULL);
    }
    *(part_end + 2) = '\0';
    text = part_end + 4;
    // Write(STDOUT_FILENO, "divide\n", 7);

    // parse the headers
    int linec = 1;
    char *lines[LINE_MAX] = {request}, *p = headers;
    while (*p != '\0')
    {
        char *line_end = strstr(p, ENDING);
        if (!line_end)
            break;
        else
        {
            *line_end = '\0';
            lines[linec++] = p;
            p = line_end + 2;
        }
    }

    char *write_p = write_buf;
    // Write(STDOUT_FILENO, "parse\n", 6);

    // handle the request line
    char req[LINE_MAX], addr[LINE_MAX], stdd[LINE_MAX];
    if (sscanf(request, "%s%s%s", req, addr, stdd) != 3)
    {
        Write(STDERR_FILENO, "\nProxy: Invalid request line:\n", 33);
        Write(STDERR_FILENO, request, strlen(request));
        Pthread_exit(NULL);
    }

    // if (strcmp(req, "GET") != 0)
    // {
    //     Write(STDERR_FILENO, "Proxy: The proxy is only able to handle GET request(not %s)\n", req);
    //     continue;
    // }

    char *real_addr, *file_path;
    if (strstr(addr, "http://") == addr)
        real_addr = addr + 7;
    else
        real_addr = addr;
    file_path = strchr(real_addr, '/');
    *file_path++ = '\0';
    if (isspace(*file_path))
    {
        Write(STDERR_FILENO, "\nProxy: Illegal file path: \n/", 29);
        Write(STDERR_FILENO, file_path, strlen(file_path));
        Pthread_exit(NULL);
    }

    // check whether the target file is in cache
    pthread_mutex_lock(&cache_mutex);

    int if_cache = 0;
    for (struct cache_node *pre = header, *p = header->next; p != NULL; pre = pre->next, p = p->next)
    {
        if (strcmp(real_addr, p->addr) == 0 && strcmp(file_path, p->file) == 0)
        {
            pre->next = p->next;
            p->next = header->next;
            header->next = p;
            Write(cfd, p->target, p->size);
            if_cache = 1;
            break;
        }
    }

    pthread_mutex_unlock(&cache_mutex);

    if (if_cache)
        Pthread_exit(NULL);

    if (strcmp(stdd, "HTTP/1.1") != 0)
    {
        Write(STDERR_FILENO, "\nProxy: The proxy is only able to handle HTTP/1.1 request(not ", 62);
        Write(STDERR_FILENO, stdd, strlen(stdd));
        Write(STDERR_FILENO, ")", 1);
        Pthread_exit(NULL);
    }

    sprintf(write_p, "%s /%s %s%s", req, file_path, "HTTP/1.0", ENDING);
    write_p = strchr(write_p, '\n') + 1;
    // Write(STDOUT_FILENO, "handle request\n", 15);

    // handle the headers by line
    int host_written = 0, agent_written = 0, connection_written = 0, proxy_written = 0;
    for (int i = 1; i < linec; i++)
    {
        p = lines[i];

        // divide the line into two parts(key : val)
        char *semicolon = strchr(p, ':');
        if (!semicolon)
        {
            Write(STDERR_FILENO, "\nProxy: Invalid line(no semicolon):\n", 36);
            Write(STDERR_FILENO, p, strlen(p));
            continue;
        }
        *semicolon = '\0';

        char *key = p, *val = semicolon + 1;
        while (*key && isspace(*key))
            key++;
        if (!*key)
        {
            *semicolon = ':';
            Write(STDERR_FILENO, "\nProxy: Invalid line(no key):\n", 30);
            Write(STDERR_FILENO, p, strlen(p));
            continue;
        }
        while (*val && isspace(*val))
            val++;
        if (!*val)
        {
            *semicolon = ':';
            Write(STDERR_FILENO, "\nProxy: Invalid line(no value):\n", 32);
            Write(STDERR_FILENO, p, strlen(p));
            continue;
        }

        // detected key type and write it to write_buf after some modification
        if (strcmp(key, "Host") == 0)
            host_written = 1;
        else if (strcmp(key, "User-Agent") == 0)
            agent_written = 1;
        else if (strcmp(key, "Connection") == 0)
            connection_written = 1;
        else if (strcmp(key, "Proxy-Connection") == 0)
            proxy_written = 1;

        sprintf(write_p, "%s: %s%s", key, val, ENDING);
        write_p = strchr(write_p, '\n') + 1;
    }
    // Write(STDOUT_FILENO, "handle headers\n", 15);

    // write the rest of must have headers
    if (!host_written)
    {
        sprintf(write_p, "Host: %s%s", addr, ENDING);
        write_p = strchr(write_p, '\n') + 1;
    }
    if (!agent_written)
    {
        sprintf(write_p, "User-Agent: %s%s", user_agent_hdr, ENDING);
        write_p = strchr(write_p, '\n') + 1;
    }
    if (!connection_written)
    {
        sprintf(write_p, "Connection: %s%s", "close", ENDING);
        write_p = strchr(write_p, '\n') + 1;
    }
    if (!proxy_written)
    {
        sprintf(write_p, "Proxy-Connection: %s%s", "close", ENDING);
        write_p = strchr(write_p, '\n') + 1;
    }
    // Write(STDOUT_FILENO, "write mheaders\n", 15);

    // write the delimiter and the text
    sprintf(write_p, ENDING);
    write_p += 2;
    sprintf(write_p, "%s", text);

    // get the address of host by name
    struct addrinfo ai, *result;
    memset(&ai, 0, sizeof(ai));
    ai.ai_flags = AI_NUMERICSERV;
    ai.ai_family = AF_INET;
    ai.ai_socktype = SOCK_STREAM;
    char *port = strchr(real_addr, ':');
    if (port)
        *port++ = '\0';
    else
        port = "80";
    Getaddrinfo(real_addr, port, &ai, &result);

    // try to set up a connection with target host
    int pfd = -1;
    struct addrinfo *rp;
    for (rp = result; rp != 0; rp = rp->ai_next)
    {
        pfd = Socket(AF_INET, SOCK_STREAM, 0);
        if (connect(pfd, rp->ai_addr, rp->ai_addrlen) != -1)
            break;
        Close(pfd);
    }
    Freeaddrinfo(result);
    if (!rp)
    {
        Write(STDERR_FILENO, "\nProxy: Unable to connect to the target server", 46);
        Pthread_exit(NULL);
    }
    // Write(STDOUT_FILENO, "set up connection\n", 18);

    // send the http message to the target host
    int n = strlen(write_buf), written;
    for (written = 0; written < n;)
    {
        int write_num = write(pfd, write_buf + written, n - written);
        if (write_num == -1)
        {
            if (errno == EINTR)
                continue;
            Write(STDERR_FILENO, "\nProxy: Write fail: ", 20);
            Write(STDERR_FILENO, strerror(errno), strlen(strerror(errno)));
            break;
        }
        written += write_num;
    }
    if (written < n)
    {
        Close(pfd);
        Pthread_exit(NULL);
    }
    // Write(STDOUT_FILENO, "send http message\n", 18);

    // receive the responce
    int content_length = 100000, header_length = 100000, fail_cnt = 0;
    char *read_p = read_buf, *line_p = read_buf, *text_start = NULL;
    int flags = 0;
    flags = fcntl(pfd, F_GETFL, NULL);
    flags |= O_NONBLOCK;
    fcntl(pfd, F_SETFL, flags);
    while (read_total < header_length + content_length)
    {
        // printf("read_total:%d\nheader_length:%d\ncontent_length:%d\nread_buf:%s\n", read_total, header_length, content_length, read_buf);
        int read_num;
        while ((read_num = read(pfd, read_p, BUF_SIZE - read_total)) < 0)
        {
            if (errno != EAGAIN && errno != EWOULDBLOCK)
            {
                Write(STDERR_FILENO, "\nProxy: Read fail.\nMessage: ", 28);
                Write(STDERR_FILENO, read_buf, strlen(read_buf));
                Close(pfd);
                Pthread_exit(NULL);
            }
            fail_cnt++;
            if (fail_cnt == 4)
            {
                Write(STDERR_FILENO, "\nProxy: Read timed out", 22);
                Close(pfd);
                Pthread_exit(NULL);
            }
            sleep(1);
        }
        fail_cnt = 0;

        // Write(STDOUT_FILENO, "read response\n", 14);
        read_total += read_num;
        read_p += read_num;
        *read_p = '\0';

        char *cl;
        while (!text_start)
        {
            // Write(STDOUT_FILENO, "find text start\n", 16);
            //  find content length
            if (content_length == 100000)
            {
                if ((cl = strstr(line_p, "Content-length")) != NULL || (cl = strstr(line_p, "Content-Length")) != NULL)
                {
                    char *cl_end;
                    if ((cl_end = strstr(cl, "\r\n")) == NULL)
                        continue;
                    content_length = atoi(cl + 15);
                }
            }
            // find next line, if empty line(headers end), set text_start
            char *line_end;
            if ((line_end = strstr(line_p, "\r\n")) != NULL)
            {
                if (line_end == line_p)
                {
                    text_start = line_end + 2;
                    header_length = text_start - read_buf;
                }
                line_p = line_end + 2;
            }
            else
                break;
        }
        // Write(STDOUT_FILENO, "next while\n", 11);
    }
    Close(pfd);

    // Write(STDOUT_FILENO, "send response\n", 14);

    if (Write(cfd, read_buf, read_total) != read_total)
    {
        Write(STDERR_FILENO, "\nProxy: Error Write", 19);
    }

    if (read_total <= MAX_OBJECT_SIZE)
    {
        *--port = ':';
        pthread_mutex_lock(&cache_mutex);

        struct cache_node *tmp;
        tmp = Malloc(sizeof(struct cache_node));
        tmp->size = read_total;
        tmp->addr = strdup(real_addr);
        tmp->file = strdup(file_path);
        tmp->target = read_buf;
        tmp->next = header->next;
        header->next = tmp;
        header->size += tmp->size;

        // shrink cache using LRU
        if (header->size > MAX_CACHE_SIZE)
        {
            // reverse the linklist
            struct cache_node *p = header->next;
            header->next = NULL;
            while (p)
            {
                struct cache_node *remain = p->next;
                p->next = header->next;
                header->next = p;
                p = remain;
            }
            // delete several nodes
            while (header->size > MAX_CACHE_SIZE)
            {
                p = header->next;
                header->next = p->next;
                header->size -= p->size;
                Free(p->target);
                Free(p->addr);
                Free(p->file);
                Free(p);
            }
            // reverse the linklist
            p = header->next;
            header->next = NULL;
            while (p)
            {
                struct cache_node *remain = p->next;
                p->next = header->next;
                header->next = p;
                p = remain;
            }
        }

        pthread_mutex_unlock(&cache_mutex);
    }
    pthread_cleanup_pop(0);
    pthread_cleanup_pop(1);
    pthread_cleanup_pop(1);
    return NULL;
}

// read code
// int read_total = 0, content_length = 0, bug = 0, header_length = 100000;
// char *read_p = read_buf, *line_p = read_buf, *text_start = NULL;
// while (read_total < header_length + content_length)
// {
//     alarm(10);
//     int read_num = read(pfd, read_p, BUF_SIZE - read_total);
//     alarm(0);
//     if (read_num <= 0)
//     {
//         fprintf(stderr, "Proxy: Read fail.\nMessage: %s\n", read_buf);
//         Close(pfd);
//         bug = 1;
//         break;
//     }
//     read_total += read_num;
//     read_p += read_num;
//     *read_p = '\0';

//     char *cl;
//     while (text_start)
//     {
//         // find content length
//         if (!content_length && (cl = strstr(line_p, "Content-Length")) != NULL)
//         {
//             char *cl_end;
//             if ((cl_end = strstr(cl, "\r\n")) == NULL)
//                 continue;
//             content_length = atoi(cl + 15);
//             break;
//         }
//         // find next line, if empty line(headers end), set text_start
//         char *line_end;
//         if ((line_end = strstr(line_p, "\r\n")) != NULL)
//         {
//             if (line_end == line_p)
//             {
//                 text_start = line_end + 2;
//                 header_length = text_start - read_buf;
//                 break;
//             }
//             line_p = line_end + 2;
//         }
//         else
//         {
//             // Illegal message
//             content_length = -1;
//             fprintf(stderr, "Proxy: Unable to find Content-Length in response message.\nMessage: %s\n", read_buf);
//             break;
//         }
//     }
//     if (content_length < 0)
//     {
//         bug = 1;
//         break;
//     }
// }

// read code
// Signal(SIGALRM, alarm_handler);
// alarm(10);
// int read_num = Read(pfd, read_buf, BUF_SIZE);
// alarm(0);
// Close(pfd);
// if (read_num <= 0)
// {
//     fprintf(stderr, "Proxy: Unable to read message from target host\n");
//     continue;
// }
// read_buf[read_num] = '\0';
// printf("%s\n", read_buf);

// if (Write(cfd, read_buf, read_num) != read_num)
// {
//     fprintf(stderr, "Proxy: Error Write\n");
// }