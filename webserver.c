/** @author heweibright@gmail.com 
 *  @date 2021/9/21 16:52
 *  Copyright (c) All rights reserved.
 */


// In my computer, glibc's version is 2.17
#define  _GNU_SOURCE
#ifdef __STDC_ALLOC_LIB__
#define __STDC_WANT_LIB_EXT2__ 1
#else
#define _POSIX_C_SOURCE 200809L
#endif
#define __USE_XOPEN2K8

// Use the Micro with condition compile
#define APPLICATION_X_WWW_URLENCODED 0
#define MULTIPART_FORM_DATA 1

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>


const int BUF_SIZE = 1024;
const int URL_SIZE = 128;
const int PATH_SIZE = 128;
const int ENV_SIZE = 128;
const int METHOD_SIZE = 64;
const int NAME_SIZE = 64;

#define SERVER_INFO "Server: CentOS-7.2, Mr.Bright\r\n"

// 1. 建立网络进行监听
int setup_network(const char *ipAddr, unsigned short port);

// 2. 接受请求并处
void handle_request(void *pclnt);

// 3. 200
void success_header(int clnt);

// 4. 400
void bad_request(int clnt);

// 5. 404
void not_found(int clnt, const char *url);

// 6. 500
void server_error(int clnt);

// 7. 501
void unimplemented(int clnt);

// 7. 发送请求的文件
void serve_file(int clnt, const char *filename);

// 8. 发送文件
void cat(int clnt, FILE *file);

// 9. 执行CGI脚本
void execute_cgi(int clnt, FILE *fp, const char *path, const char *method, const char *query_string);

// 10. 错误处理
void error_handling(const char *msg);


int main(int argc, char *argv[]) {
    if (argc < 2 || argc > 3) {
        printf("Usage:\n 1. %s <IP> <Port>\n 2. %s <Port>\n", argv[0], argv[0]);
        exit(1);
    }
    int serv_sock, clnt_sock;
    struct sockaddr_in clnt_addr;
    socklen_t clnt_addr_len = sizeof(clnt_addr);
    unsigned short port = argc == 2 ? atoi(argv[1]) : atoi(argv[2]);
    pthread_t pthread;
    if (argc == 2)
        serv_sock = setup_network(NULL, port);
    else
        serv_sock = setup_network(argv[1], port);
    printf("http server socket is %d\n", serv_sock);
    printf("http running on port %d\n", port);
    while (1) {
        clnt_sock = accept(serv_sock, (struct sockaddr *) &clnt_addr, &clnt_addr_len);
        if (clnt_sock == -1)
            error_handling("accept()");
        printf("New Connection... IP: %s, Port: %d\n", inet_ntoa(clnt_addr.sin_addr), clnt_addr.sin_port);
        // TODO：多线程竞争clnt_sock这一资源如何处理？上锁？信号量？(FIXED)
        //! 参考了一种处理方式：采用头文件<stdint.h>中的intptr_t，关于它的具体内容自行查看头文件 or stfw！
        if (pthread_create(&pthread, NULL, (void *) handle_request, (void *) (intptr_t) clnt_sock) != 0)
            error_handling("pthread_create()");
        if (pthread_join(pthread, NULL) != 0)
            error_handling("pthread_join()");
    }
    close(serv_sock);
    return 0;
}

/**
 * 如果ipAddr为NULL，则使用INADDR_ANY，否则使用指定ipAddr；
 * 如果当前port被占用，则返回-1；
 * */
int setup_network(const char *ipAddr, unsigned short port) {
    int serv_sock;
    struct sockaddr_in serv_addr;

    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1)
        error_handling("socket()");

    int option = 1, optlen = sizeof(option);
    setsockopt(serv_sock, SOL_SOCKET, SO_REUSEADDR, (void *) &option, optlen);

    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    if (ipAddr == NULL)
        serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    else
        serv_addr.sin_addr.s_addr = htonl(atoi(ipAddr));
    serv_addr.sin_port = htons(port);

    if (bind(serv_sock, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) == -1)
        error_handling("bind()");

    // TODO: 修改了listen的数值！但其实我觉得问题不出在这里！(FIXED)
    if (listen(serv_sock, 1) == -1)
        error_handling("listen()");
    return serv_sock;
}

void handle_request(void *pclnt) {
    int clnt = (intptr_t) pclnt;
    char buf[BUF_SIZE];
    char *line = NULL;
    size_t buf_len = sizeof(buf);
    char url[URL_SIZE];
    char path[PATH_SIZE];
    char method[METHOD_SIZE];
    // 查询字符串(GET方法的URL后一串参数)
    char *query_string = NULL;
    // 存储文件相关信息：man 2 stat, man 3 stats
    struct stat status;
    // 判断是否是可执行文件cgi
    int cgi = 0;
    // HTTP Request Message 1st Line: Method Url Version\r\n
    FILE *file = fdopen(clnt, "r");
    // getline内存需要记得释放，why？ stfw！
    ssize_t len = getline(&line, &buf_len, file);
    strcpy(buf, line);
    free(line);

    if (!feof(file) && len < 0)
        error_handling("getline()");

    //!!! fclose会将clnt全关闭！踩得坑！果然认真学过的东西还是忘的飞快！好在排坑时想起来了。留下这个注释是为了提示自己和大家！
    // fclose(file);
    strtok(buf, " ");
    strcpy(method, buf);
    strcpy(url, strtok(NULL, " "));

    // 未使用GET或POST方法
    if (strcmp(method, "GET") != 0 && strcmp(method, "POST") != 0) {
        unimplemented(clnt);
        return;
    }
    // GET方法：URL可能带有'?'
    // e.g: http://www.*****.***/***?key1=val1&key2=val2...
    // 我们认为用户输错了后面的参数，用户可能希望访问http://www.******.***/***页面，因此直接去掉后面的传参部分
    if (strcmp(method, "GET") == 0) {
        for (size_t i = 0; i < strlen(url); ++i) {
            if (url[i] == '?') {
                url[i] = '\0';
                query_string = url + 1;
                break;
            }
        }
    }

    // html文件存放在httpdocs中，cgi脚本存放在cgi目录中
    if (strlen(url) > 4 && strncmp(url, "/cgi", 4) == 0)
        sprintf(path, "%s", url + 1);
    else if (strlen(url) > 6 && strncmp(url, "/files", 6) == 0)
        sprintf(path, "%s", url + 1);
    else
        sprintf(path, "httpdocs%s", url);

    // 如果用户只输入了IP + Port，并没有指定HTML文件，我们默认展示readme.html
    if (path[strlen(path) - 1] == '/')
        strcat(path, "readme.html");

    printf("path: %s\n", path);
    fflush(stdout);
    if (stat(path, &status) == -1) {
        // 实际上应该输出：http://ip:port/path
        // 当前的url只有path
        // TODO: 需要根据具体内容再进一步修改 是否需要读取完所有request中的内容？(FIXED)
        //! 实际上并不需要！因为我们目前只用到了第一行！后面的缓冲区会在这次读取后close clnt时释放！
        perror("stat()");
        not_found(clnt, path);
    } else {
        // 如果当前path是目录，说明用户输入不够完美，我们默认他访问readme.html
        if ((status.st_mode & __S_IFMT) == S_IFDIR)
            strcat(path, "/readme.html");

        // 可执行文件(cgi) 文件拥有者、用户组、其他用户具有可执行权
        if ((status.st_mode & S_IXUSR) || (status.st_mode & S_IXGRP) || (status.st_mode & S_IXOTH))
            cgi = 1;
        if (!cgi)
            serve_file(clnt, path);
        else {
            // TODO: 之前按照对tinyhttpd的阅读理解，采用的是相同的方式传参！但后来我有使用stdio.h的getline函数
            //  因此这里我传递file进入读取数据，否则再次通过fdopen打开clnt可能是因为读取文件的指针偏移的原因？并没能正确读出数据！而且也有多余的开销。
            execute_cgi(clnt, file, path, method, query_string);
            printf("execute successful\n");
        }
    }
    fclose(file);  // 等价与close(clnt);
    printf("connection with client %d closed.\n", clnt);
    return;
}

/*
 * HTTP/1.1 400 BAD REQUEST\r\n
 * Content-Type: text/html\r\n
 * \r\n
 * <HTML><TITLE>400 Bad Request</TITLE>\r\n
 * <BODY><P>The plain HTTP request sent to HTTP port or other error.</P>\r\n
 * </BODY></HTML>\r\n
 * \r\n
 * */
void bad_request(int clnt) {
    char buf[BUF_SIZE];
    sprintf(buf, "HTTP/1.1 400 BAD REQUEST\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>404 Bad Request</TITLE>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The plain HTTP request sent to HTTP port or other error.</P>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
}

/*
 * HTTP/1.1 200 OK\r\n
 * Server: {server info}\r\n
 * Connection: Keep-alive\r\n
 * Content-Type: text/octet\r\n
 * \r\n
 * */
void success_header(int clnt) {
    char buf[BUF_SIZE];
    sprintf(buf, "HTTP/1.1 200 OK\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, SERVER_INFO);
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Connection: Keep-alive\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
}

/*
 * HTTP/1.1 404 NOT FOUND\r\n
 * Server: {server info}\r\n
 * Connection: close\r\n
 * Content-Type: text/html\r\n
 * \r\n
 * <HTML><TITLE>Not Found</TITLE>\r\n
 * <BODY><P>The requested URL {url} was not found on this server.</P>\r\n
 * </BODY></HTML>\r\n
 * \r\n
 * */
void not_found(int clnt, const char *url) {
    char buf[BUF_SIZE];
    sprintf(buf, "HTTP/1.1 404 NOT FOUND\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, SERVER_INFO);
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>404 Not Found</TITLE>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>The requested URL %s was not found on this server.</P>\r\n", url);
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
}

/*
 * HTTP/1.1 500 Internal Server Error\r\n
 * Server: {server info}\r\n
 * Connection: close\r\n
 * Content-Type: text/html\r\n
 * \r\n
 * <HTML><TITLE>500 Internal Error</TITLE>\r\n
 * <BODY><P>The server encountered an unexpected error that prevented it from fulfilling the request.</P>\r\n
 * </BODY></HTML>\r\n
 * \r\n
 * */
void server_error(int clnt) {
    char buf[BUF_SIZE];
    sprintf(buf, "HTTP/1.1 500 Internal Server Error\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, SERVER_INFO);
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>500 Internal Server Error</TITLE>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf,
            "<BODY><P>The server encountered an unexpected error that prevented it from fulfilling the request.</P>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
}

/*
 * HTTP/1.1 501 Method Not Implemented\r\n
 * Server: {server info}\r\n
 * Connection: close\r\n
 * Content-Type: text/html\r\n
 * \r\n
 * <HTML><TITLE>500 Method Not Implemented</TITLE>\r\n
 * <BODY><P>HTTP request method not implemented.</P>\r\n
 * </BODY></HTML>\r\n
 * \r\n
 * */
void unimplemented(int clnt) {
    char buf[BUF_SIZE];
    sprintf(buf, "HTTP/1.1 501 Method Not Implemented\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, SERVER_INFO);
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Connection: close\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "Content-Type: text/html\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "<HTML><TITLE>501 Method Not Implemented</TITLE>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "<BODY><P>HTTP request method not implemented.</P>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "</BODY></HTML>\r\n");
    send(clnt, buf, strlen(buf), 0);
    sprintf(buf, "\r\n");
    send(clnt, buf, strlen(buf), 0);
}

void cat(int clnt, FILE *file) {
    //! 为什么这里采用fgets可以，而采用fread不行呢？
    // 你需要了解fread & fgets两者的区别 ---> 移步手册
    // 简短版：fread相当于n次调用fgetc，并将结果转译到unsigned char数组中
    // 因此如果我们想要保证读取正确，则需要使用"wb"的方式打开文件！
    // TODO: 这里就留给你自己发挥吧！换成fread，保证程序正确！
    char buf[BUF_SIZE];
    while (fgets(buf, sizeof(buf), file) != NULL) {
        send(clnt, buf, strlen(buf), 0);
    }

}

void serve_file(int clnt, const char *filename) {
    // TODO: 经过tinyhttpd代码测试，一定要将clnt的缓存去掉，否则会充值连接导致问题！(FIXED)
    //! 注意这里file是NULL，也就是说打开失败了？可能clnt被close过？还是其他原因
    //! 果然原因是在之前对clnt转FILE*后，进行了fclose，导致clnt被关闭了
    FILE *file = fopen(filename, "r");
    if (file == NULL)
        not_found(clnt, filename);
    else {
        success_header(clnt);
        cat(clnt, file);
    }
    fclose(file);
}

void execute_cgi(int clnt, FILE *fp, const char *path, const char *method, const char *query_string) {
    char buf[BUF_SIZE];
    int in_pipes[2];
    int out_pipes[2];

    // 用来传递环境变量！
    // 父进程和子进程不会共享同样的环境变量，而我们
#if MULTIPART_FORM_DATA
    int env_pipes[2];
    if (pipe(env_pipes) < 0)
        perror("pipe()");

    // TODO：设置clnt为非阻塞
    int flags;
    if ((flags = fcntl(clnt, F_GETFL, NULL)) < 0)
        perror("fcntl()");
    if (fcntl(clnt, F_SETFL, flags | O_NONBLOCK) == -1)
        perror("fcntl()");
#endif

    int content_length = 0;
    FILE *file = fp;

    // 动态请求
    if (strcmp(method, "GET") == 0) {
        while (strcmp(buf, "\r\n") != 0) {
            fgets(buf, sizeof(buf), file);
        }
    } else {
#if APPLICATION_X_WWW_URLENCODED
        // Content-Lenght: xxx\r\n
        while (file != NULL && !feof(file) && strncmp(buf, "Content-Length: ", 16) != 0) {
            fgets(buf, sizeof(buf), file);
        }
        content_length = atoi(&buf[16]);
        if (content_length == -1) {
            bad_request(clnt);
            return;
        }
#endif
    }

    sprintf(buf, "HTTP/1.0 200 OK\r\n");
    send(clnt, buf, strlen(buf), 0);

    // 不能正常申请pipe，因此为服务器内部错误
    if (pipe(in_pipes) < 0 || pipe(out_pipes) < 0) {
        server_error(clnt);
        return;
    }
    pid_t pid = fork();
    if (pid < 0) {
        server_error(clnt);
        return;
    }
    // 子进程：执行cgi脚本，同父进程进行数据交互
    // 父进程：与子进行数据交互，并将数据传递给client(浏览器)
    if (pid == 0) {
        char meth_env[ENV_SIZE];
        char query_env[ENV_SIZE];
        char length_env[ENV_SIZE];

        // 将文件描述符0，1分别重定向为in_pipes[0], out_pipes[1]
        // cgi脚本执行默认输入输出为stdio, stdout，其实是默认为0, 1文件描述符所指向的具体文件对象
        // 0 1 2：默认为stdio stdout stderr的文件描述符
        dup2(in_pipes[0], 0);
        dup2(out_pipes[1], 1);

        // 因为fork时，会将所有所有资源进行复制，所以这里需要关闭以下文件描述符
        close(in_pipes[1]);
        close(out_pipes[0]);

#if MULTIPART_FORM_DATA
        /** 假设post数据以multipart/form-data模式来编码传输 **/
        // 因为fork时，会将所有所有资源进行复制，所以这里需要关闭以下文件描述符
        close(in_pipes[1]);
        /*************************************************************/
#endif

        sprintf(meth_env, "REQUEST_METHOD=%s", method);
        putenv(meth_env);
        if (strcmp(method, "GET") == 0) {
            sprintf(query_env, "QUERY_STRING=%s", query_string);
            putenv(query_env);
        } else {
#if APPLICATION_X_WWW_URLENCODED
            /** 假设post数据默认为application/x-www-urlencoded模式来编码传输 **/
            sprintf(length_env, "CONTENT_LENGTH=%d", content_length);
            putenv(length_env);
            /*************************************************************/

#elif MULTIPART_FORM_DATA
            /** 假设post数据以multipart/form-data模式来编码传输 **/
            read(env_pipes[0], length_env, sizeof(length_env));
            putenv(length_env);
            /*************************************************************/
#endif
        }

        // 需要执行的文件的路径在path，执行方法path，参数NULL。
        // e.g: /home/bright/C++ test NULL，相当于在这个目录下调用test
        // cgi脚本是默认从0，1文件描述符所指向的文件端读取和写入数据

        // TODO: 原来是execl执行脚本出问题了！所以传入的参数问题？ (FIXED)
        //! ==>理解execl和execlp的区别，其实应该直接使用execl也可以？没测试了，留给你来做吧？我懒！===>好吧，我还是尝试了，可以的哟！
        //! 现在发现是我python cgi脚本的原因，因为它会通过 #!/usr/bin/python来找到对应解释器执行！
        //! 而我之前通过python格式化了一下变成了 # ！usr/bin/python，因此找不到解释器！
        if (execl(path, path, NULL) == -1)
            error_handling("execl()");
        // 新添加了两行close语句！！！！edit 2021-09-30
        close(in_pipes[0]);
        close(out_pipes[1]);
        exit(EXIT_SUCCESS);
    } else {
        close(in_pipes[0]);
        close(out_pipes[1]);
        char c = '0';
        if (strcmp(method, "POST") == 0) {

#if APPLICATION_X_WWW_URLENCODED
            /** 假设post数据默认为application/x-www-urlencoded模式来编码传输 **/
            //! 该while循环目的是为了后面直接读取post表单数据
            while (file != NULL && fgets(buf, sizeof(buf), file) != NULL && strcmp(buf, "\r\n") != 0);
            for (int i = 0; i < content_length; ++i) {
                // Assuming that post data length smaller than sizeof(buf)!
                fscanf(file, "%c", &c);
                // TODO: Broken pipe？为什么说in_pipes被关闭了？不应该鸭？(FIXED)
                //! 通过搜索broken pipe可能的原因，发现，可能是因为一次性写入内容太多，而没有在另一端进行接收从而导致pipe broken！
                write(in_pipes[1], &c, 1);
            }
             /*************************************************************/

#elif MULTIPART_FORM_DATA
            /** 假设post数据以multipart/form-data模式来编码传输 **/
            close(env_pipes[0]);

            char *firstPos = NULL;
            char *line = NULL;
            size_t len;
            char boundary[BUF_SIZE];

            // 为什么通过while循环里加if判断？主要目的：释放动态获取的空间！
            while (file != NULL && getline(&line, &len, file)) {
                if ((firstPos = strstr(line, "boundary=")) != NULL)
                    break;
                if (line != NULL)
                    free(line);
                line = NULL;
            }
            strcpy(boundary, firstPos + strlen("boundary="));
            if (line != NULL)
                free(line);

            // data存储返回给cgi脚本展示的内容
            char data[BUF_SIZE];
            memset(data, 0, sizeof(data));
            char filepath[PATH_SIZE];
            strcpy(filepath, "files/");

            const char *CONTENT_DISPOSITION = "Content-Disposition: form-data;";

            char __boundary[BUF_SIZE];
            char __boundary__[BUF_SIZE];
            strcpy(__boundary, "--");
            strcat(__boundary, boundary);

            strcpy(__boundary__, "--");
            strcat(__boundary__, boundary);
            __boundary__[strlen(__boundary__) - 2] = '\0';
            strcat(__boundary__, "--\r\n");

            // TODO: 因为socket是流，会引起阻塞！因此解决方案是将套接字设为非阻塞的。
            while (!feof(file) && fgets(buf, sizeof(buf), file) != NULL) {
                if (strcmp(buf, __boundary) == 0)
                    continue;
                if (strcmp(buf, __boundary__) == 0)
                    break;
                if (strncmp(buf, CONTENT_DISPOSITION, strlen(CONTENT_DISPOSITION)) == 0) {
                    char name[NAME_SIZE];
                    char value[BUF_SIZE];
                    if (strstr(buf, "filename") == NULL) {
                        strtok(buf, "\"");
                        strcpy(name, strtok(NULL, "\""));
                        fgets(buf, sizeof(buf), file);
                        fgets(value, sizeof(value), file);
                        value[strlen(value) - 2] = '\0';
                        if (data[0] != '\0')
                            strcat(data, "&");
                        strcat(data, name);
                        strcat(data, "=");
                        strcat(data, value);
                    } else {
                        strtok(buf, "\"");
                        strcpy(name, strtok(NULL, "\""));
                        strtok(NULL, "\"");
                        strcpy(value, strtok(NULL, "\""));
                        if (data[0] != '\0')
                            strcat(data, "&");
                        strcat(data, name);
                        strcat(data, "=");
                        strcat(data, value);
                        // 获取文件名
                        strcat(filepath, value);

                        fgets(buf, sizeof(buf), file);
                        fgets(buf, sizeof(buf), file);

                        printf("filepath: %s\n", filepath);
                        fflush(stdout);
                        if (strcmp(filepath, "/home/bright/GitRepo/MyTinyWebServer/") != 0) {
                            FILE *usrfile = fopen(filepath, "w");
                            if (usrfile == NULL)
                                error_handling("fopen()");
                            while (fgets(buf, sizeof(buf), file) != NULL) {
                                if (strcmp(buf, "\r\n") == 0)
                                    continue;
                                if (strcmp(buf, __boundary__) == 0)
                                    break;
                                fwrite(buf, sizeof(char), strlen(buf), usrfile);
                            }
                            // 关闭文件
                            fclose(usrfile);
                            printf("Write file successfully\n");
                        }
                    }
                }
            }
            // 将环境变量传递给子进程
            sprintf(buf, "CONTENT_LENGTH=%lu", strlen(data));
            if (write(env_pipes[1], buf, strlen(buf)) < 0)
                perror("write()");
            close(env_pipes[1]);

            content_length = strlen(data);
            write(in_pipes[1], data, content_length);
            /** 这种方式和上面等价！
            for (int i = 0; i < content_length; ++i) {
                // TODO: Broken pipe？为什么说in_pipes被关闭了？不应该鸭？(FIXED)
                //! 通过搜索broken pipe可能的原因，发现，可能是因为一次性写入内容太多，而没有在另一端进行接收从而导致pipe broken！
                write(in_pipes[1], &data[i], 1);
            }
             */
            /*************************************************************/
#endif
        }
        // TODO: 我现在发现问题可能是：我使用过fdopen(clnt)导致clnt不能正确读取数据了？(FIXED)
        // 因为尽管下面从out_pipes[0]中不能正常读取数据，我也通过clnt写入了数据。
        // 大概率是因为我通过fdopen(clnt)后，导致浏览器端clnt不能正确读取数据，因为我通过了如下方式向clnt写入内容！
        //! 但是细想，也不对，因为我GET请求，不执行cgi脚本时，它也会fdopen(clnt)鸭！所以到底那一步有问题？===>所以并不是不能读取数据了！
        // debug时：发现此时传送数据没问题，但是发现似乎不能正确读取数据？===>可以的，但是为什么我在debug时，却不能正常读取数据？这个点我有丢丢没明白！
        // 读取cgi脚本返回数据，发送给浏览器。
        while (read(out_pipes[0], &c, 1)) {
            send(clnt, &c, 1, 0);
        }
        // 运行结束关闭
        close(out_pipes[0]);
        close(in_pipes[1]);
        waitpid(pid, 0, 0);
    }
}

void error_handling(const char *msg) {
    perror(msg);
    exit(1);
}

