#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <signal.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>

#define BUF_SIZE 1024

void error_handing(char *m) {
    fputs(m, stderr);
    fputc('\n', stderr);
    exit(1);
}

void read_child_process(int sig) {
    int status;
    pid_t pid = waitpid(-1, &status, WNOHANG);
    if (WEXITSTATUS(status) == 1) {
        printf("process file closed! removed pid = %d \n", pid);
    } else if (WEXITSTATUS(status) == 2) {
        printf("process echo closed! removed pid = %d \n", pid);
    } else {
        printf("removed pid = %d \n", pid);
    }
}

int main(int argc, char*argv[]) {
    int serv_sock, clnt_sock;
    struct sockaddr_in serv_addr, clnt_addr;
    
    if (argc != 2) {
    	printf("usage: %s <port>\n", argv[0]);
    	exit(1);
    }

    struct sigaction act;  // 僵死进程处理器
    act.sa_handler = read_child_process;
    sigemptyset(&act.sa_mask);
    act.sa_flags = 0;
    sigaction(SIGCHLD, &act, 0);  // 效果类似于signal，只要有子进程创建就调用act里的handler
    // signal(SIGCHLD, read_child_process);

    // 创建服务器进程
    serv_sock = socket(PF_INET, SOCK_STREAM, 0);
    if (serv_sock == -1) {
    	error_handing("serv_sock error! ");
    }
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    serv_addr.sin_port = htons(atoi(argv[1]));

    if (bind(serv_sock, (struct sockaddr*) &serv_addr, sizeof(serv_addr)) == -1) {
    	error_handing("bind() error! ");
    }

    if (listen(serv_sock, 5) == -1) {
    	error_handing("listen() error! ");
    }

    int fds[2];
    pipe(fds);
    pid_t pid = fork();

    if (pid == 0) {
        FILE *fp = fopen("echomsg.txt", "wt");
        char msgbuf[BUF_SIZE];
        int i, len;
        for (i = 0; i < 10; ++i) {
            len = read(fds[0], msgbuf, BUF_SIZE);
            fwrite((void*) msgbuf, 1, len, fp);
        }
        fclose(fp);
        return 1;
    }

    while (1) {
        socklen_t addr_size = sizeof(clnt_addr);
        clnt_sock = accept(serv_sock, (struct sockaddr*) &clnt_addr, &addr_size);

        if (clnt_sock == -1) {
            continue;
        } else {
            puts("new client connected! ");
        }

        pid = fork();
        if (pid == 0) {  // 子进程执行模块
            close(serv_sock);
            char buf[BUF_SIZE];
            int str_len;
            while ((str_len = read(clnt_sock, buf, BUF_SIZE)) != 0) {
                write(clnt_sock, buf, str_len);
                write(fds[1], buf, str_len);
            }
            close(clnt_sock);
            puts("client disconnected! ");
            return 2;
        } else {
            close(clnt_sock);
        }
    }

    close(serv_sock);
    return 0;
}
