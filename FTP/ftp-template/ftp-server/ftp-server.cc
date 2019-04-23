#include <stdlib.h>
#include <iostream>
#include <sys/types.h>
#include <sys/sendfile.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <stdint.h>

//  COMMANDS
#define CMD_USER 0
#define CMD_QUIT 1
#define CMD_PORT 2
#define CMD_TYPE 3
#define CMD_MODE 4
#define CMD_STRU 5
#define CMD_STOR 6
#define CMD_RETR 7
#define CMD_NOOP 8
#define CMD_LIST 9
#define CMD_MKD 10
#define CMD_MISSING 50 //not implemented
#define CMD_INVALID 100  //doesn't exist

int cmdInt(char *cmd)
{
    if (strcmp(cmd, "QUIT") == 0) return CMD_QUIT;
    if (strcmp(cmd, "USER") == 0) return CMD_USER;
    if (strcmp(cmd, "PORT") == 0) return CMD_PORT;
    if (strcmp(cmd, "TYPE") == 0) return CMD_TYPE;
    if (strcmp(cmd, "MODE") == 0) return CMD_MODE;
    if (strcmp(cmd, "STRU") == 0) return CMD_STRU;
    if (strcmp(cmd, "STOR") == 0) return CMD_STOR;
    if (strcmp(cmd, "RETR") == 0) return CMD_RETR;
    if (strcmp(cmd, "NOOP") == 0) return CMD_NOOP;
    if (strcmp(cmd, "LIST") == 0) return CMD_LIST;
    if (strcmp(cmd, "MKD") == 0) return CMD_MKD;
    if (strcmp(cmd, "PASS") == 0 || strcmp(cmd, "ACCT") == 0 || strcmp(cmd, "CWD") == 0 || strcmp(cmd, "CDUP") == 0 || strcmp(cmd, "SMNT") == 0 || strcmp(cmd, "REIN") == 0 || strcmp(cmd, "PASV") == 0 || strcmp(cmd, "STOU") == 0 || strcmp(cmd, "APPE") == 0 || strcmp(cmd, "ALLO") == 0 || strcmp(cmd, "REST") == 0 || strcmp(cmd, "RNFR") == 0 || strcmp(cmd, "RNTO") == 0 || strcmp(cmd, "ABOR") == 0 || strcmp(cmd, "DELE") == 0 || strcmp(cmd, "RMD") == 0 || strcmp(cmd, "PWD") == 0 || strcmp(cmd, "NLST") == 0 || strcmp(cmd, "SITE") == 0 || strcmp(cmd, "SYST") == 0 || strcmp(cmd, "STAT") == 0 || strcmp(cmd, "HELP") == 0)
    {
        return CMD_MISSING;
    }
    return CMD_INVALID;
}

int main(int argc, char **argv)
{
    struct sockaddr_in sa;
    int socket_FD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    int thisPort;
    if (socket_FD == -1)
    {
        perror("cannot create socket");
        exit(EXIT_FAILURE);
    }

    memset(&sa, 0, sizeof sa);

    sa.sin_family = AF_INET;
    thisPort = strtol(argv[1], NULL, 10);
    sa.sin_port = htons(thisPort);
    sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    if (bind(socket_FD, (struct sockaddr *)&sa, sizeof sa) == -1)
    {
        perror("bind failed");
        close(socket_FD);
        exit(EXIT_FAILURE);
    }

    if (listen(socket_FD, 10) == -1)
    {
        perror("listen failed");
        close(socket_FD);
        exit(EXIT_FAILURE);
    }

    //  Command vars
    int cmdNum;
    char cmd_buffer[100];
    char *cmd, *next;
    const char *resp;
    const char *CWD = "/home/user/Desktop/cs4414/FTP/ftp-template/ftp-server/"; // CWD in my VM

    //  Socket
    int data_FD, bin;
    struct sockaddr_in sa_data;

    //  IPaddr
    int i, data_prt;
    const char *ptr;
    char IP_str[INET_ADDRSTRLEN];

    //  Read/Write
    int fd, fd2, tmpW, tmpR;
    struct stat fst;

    //  LIST, STOR, RETR
    bool flgRead;
    int check, status, bytesRead;
    char exec_buffer[1024], read_buffer[1024];

    //  state flags
    bool FLG_auth, FLG_quit, FLG_data, FLG_type;

    for (;;)
    {
        int connect_FD = accept(socket_FD, NULL, NULL);
        FLG_auth = false;
        FLG_quit = false;
        FLG_data = false;
        FLG_type = false;

        if (0 > connect_FD)
        {
            perror("accept failed");
            close(socket_FD);
            exit(EXIT_FAILURE);
        }
        perror("incoming connection");

        resp = "220 Service ready for new user.\r\n";
        memcpy(cmd_buffer, resp, strlen(resp) + 1);
        tmpW = write(connect_FD, cmd_buffer, strlen(resp));

        for (;;)
        {
            memset(cmd_buffer, '\0', 100);

            tmpR = read(connect_FD, cmd_buffer, 99);
            perror(cmd_buffer);

            cmd = strtok(strdup(cmd_buffer), " \r\n");
            cmdNum = cmdInt(cmd);

            switch (cmdNum)
            {
            case CMD_QUIT:
                resp = "221 Service closing control connection.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                FLG_quit = true;
                break;

            case CMD_USER:
                resp = "230 User logged in, proceed.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                FLG_auth = true;
                break;

            case CMD_PORT:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                if (!FLG_data)
                {
                    IP_str[0] = '\0';
                    for (i = 0; i < 4; i++)
                    {
                        ptr = strtok(NULL, ",");
                        strcat(IP_str, ptr);
                        if (i < 3)
                            strcat(IP_str, ".");
                    }
                    data_prt = strtol(strtok(NULL, ","), NULL, 10);
                    data_prt <<= 8;
                    data_prt += strtol(strtok(NULL, "\r\n"), NULL, 10);
                    data_FD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
                    if (data_FD == -1)
                    {
                        perror("cannot create socket");
                        exit(EXIT_FAILURE);
                    }

                    memset(&sa_data, 0, sizeof sa_data);

                    sa_data.sin_family = AF_INET;
                    sa_data.sin_port = htons(data_prt);
                    bin = inet_pton(AF_INET, IP_str, &sa_data.sin_addr);

                    if (connect(data_FD, (struct sockaddr *)&sa_data, sizeof sa_data) == -1)
                    {
                        perror("connect failed");
                        close(connect_FD);
                        close(socket_FD);
                        close(data_FD);
                        exit(EXIT_FAILURE);
                    }
                    printf("socket connected");
                }
                FLG_data = true;
                resp = "200 Command Successful.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;

            case CMD_TYPE:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                next = strtok(NULL, "\r\n");
                if (strcmp(next, "I") == 0)
                {
                    FLG_type = true;
                    resp = "200 Command Successful.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                }
                else
                {
                    resp = "501 Syntax error in parameters or arguments.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                }
                break;

            case CMD_MODE:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                next = strtok(NULL, "\r\n");
                if (strcmp(next, "S") == 0)
                {
                    resp = "200 Command Successful.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                }
                else
                {
                    resp = "501 Syntax error in parameters or arguments.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                }
                break;

            case CMD_STRU:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                next = strtok(NULL, "\r\n");
                if (strcmp(next, "F") == 0)
                {
                    resp = "200 STRU Command Successful.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                }
                else
                {
                    resp = "501 Syntax error in parameters or arguments.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                }
                break;

            case CMD_STOR:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                if (!FLG_type)
                {
                    resp = "451 Requested action aborted: local error in processing.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }

                next = strtok(NULL, "\r\n");
                resp = "125 Data connection already open; transfer starting.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));

                if (next != NULL) strcat(read_buffer, next);

                fd = open(read_buffer, O_RDWR | O_CREAT | O_TRUNC, 0600);
                flgRead = false;
                bytesRead = 0;

                for (;;)
                {
                    if ((bytesRead = recv(data_FD, next, 1024, 0)) < 0)
                    {
                        perror("error in reading\n");
                        break;
                    }
                    else if (bytesRead < 1024)
                    {
                        flgRead = true;
                    }
                    tmpW = write(fd, next, bytesRead);

                    if (flgRead)
                        break;
                }
                close(data_FD);
                close(fd);
                FLG_data = false;

                resp = "226 Closing data connection. Requested file transfer successful.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;

            case CMD_RETR:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                if (!FLG_type)
                {
                    resp = "451 Requested action aborted: local error in processing.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                next = strtok(NULL, "\r\n");

                resp = "125 Data connection already open; transfer starting.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));

                memset(read_buffer, 0, 1024);
                if (next != NULL)
                {
                    strcat(read_buffer, CWD);
                    strcat(read_buffer, next);
                }
                fd = open(read_buffer, O_RDONLY);
                fstat(fd, &fst);
                check = sendfile(data_FD, fd, NULL, fst.st_size);
                close(data_FD);
                close(fd);
                FLG_data = false;

                resp = "226 Closing data connection. Requested file transfer successful.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;

            case CMD_LIST:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                next = strtok(NULL, "\r\n");
                resp = "125 Data connection already open; transfer starting.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));

                fd2 = dup(STDOUT_FILENO);
                dup2(data_FD, STDOUT_FILENO);

                memset(exec_buffer, 0, 1024);

                strcat(exec_buffer, "ls -l ");
                if (next != NULL)
                {
                    strcat(exec_buffer, CWD);
                    strcat(exec_buffer, next);
                }

                status = system(exec_buffer);

                close(data_FD);
                FLG_data = false;
                close(STDOUT_FILENO);
                dup2(fd2, STDOUT_FILENO);

                resp = "226 Closing data connection. Requested file transfer successful.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;

            case CMD_NOOP:
                resp = "200 NOOP Command Successful.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;

            case CMD_MKD:
                if (!FLG_auth)
                {
                    resp = "530 Not logged in.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                next = strtok(NULL, "\r\n");
                memset(exec_buffer, 0, 1024);
                strcat(exec_buffer, "mkdir ");
                if (strstr(next, "..") != NULL)
                {
                    resp = "501 Syntax error in parameters or arguments.\r\n";
                    memcpy(cmd_buffer, resp, strlen(resp) + 1);
                    tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                    break;
                }
                else if (next != NULL)
                {
                    strcat(exec_buffer, CWD);
                    strcat(exec_buffer, next);
                }
                status = system(exec_buffer);
                if (status != 0)
                {
                    resp = "501 Directory Exists\r\n";
                }
                else
                {
                    resp = "257 Directory Created Successfully\r\n";
                }
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;

            case CMD_MISSING:
                resp = "502 Command not implemented.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;

            default:
                resp = "500 Syntax error, command unrecognized.\r\n";
                memcpy(cmd_buffer, resp, strlen(resp) + 1);
                tmpW = write(connect_FD, cmd_buffer, strlen(resp));
                break;
            }
            if (FLG_quit)
            {
                break;
            }
        }

        perror("closing connection\n");

        if (FLG_data)
        {
            shutdown(data_FD, SHUT_RDWR);
            close(data_FD);
        }

        if (shutdown(connect_FD, SHUT_RDWR) == -1)
        {
            perror("shutdown failed");
            close(connect_FD);
            close(socket_FD);
            exit(EXIT_FAILURE);
        }
        close(connect_FD);
    }

    close(socket_FD);
    if (tmpW < 0 && tmpR < 0 && status < 0 && bin < 0 && check < 0)
    {
        perror("Status error\n");
    }
    return EXIT_SUCCESS;
}
