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

#define USER_C 0
#define QUIT_C 1
#define PORT_C 2
#define TYPE_C 3
#define MODE_C 4
#define STRU_C 5
#define STOR_C 6
#define RETR_C 7
#define NOOP_C 8
#define LIST_C 9
#define MKD_C 10
#define OTHER_C 50 //represents any non-implemented commands to display message
#define BAD_C 100 //any input that is not an existing command

int cmdToInt(char *cmd){
  if(strcmp(cmd, "QUIT") == 0)
    return QUIT_C;
  if(strcmp(cmd, "USER") == 0)
    return USER_C;
  if(strcmp(cmd, "PORT") == 0)
    return PORT_C;
  if(strcmp(cmd, "TYPE") == 0)
    return TYPE_C;
  if(strcmp(cmd, "MODE") == 0)
    return MODE_C;
  if(strcmp(cmd, "STRU") == 0)
    return STRU_C;
  if(strcmp(cmd, "STOR") == 0)
    return STOR_C;
  if(strcmp(cmd, "RETR") == 0)
    return RETR_C;
  if(strcmp(cmd, "NOOP") == 0)
    return NOOP_C;
  if(strcmp(cmd, "LIST") == 0)
    return LIST_C;
  if(strcmp(cmd, "MKD") == 0)
    return MKD_C;
  if(strcmp(cmd, "PASS") == 0 || strcmp(cmd, "ACCT") == 0 || strcmp(cmd, "CWD") == 0 || strcmp(cmd, "CDUP") == 0 || strcmp(cmd, "SMNT") == 0 || strcmp(cmd, "REIN") == 0 || strcmp(cmd, "PASV") == 0 || strcmp(cmd, "STOU") == 0 || strcmp(cmd, "APPE") == 0 || strcmp(cmd, "ALLO") == 0 || strcmp(cmd, "REST") == 0 || strcmp(cmd, "RNFR") == 0 || strcmp(cmd, "RNTO") == 0 || strcmp(cmd, "ABOR") == 0 || strcmp(cmd, "DELE") == 0 || strcmp(cmd, "RMD") == 0 || strcmp(cmd, "PWD") == 0 || strcmp(cmd, "NLST") == 0 || strcmp(cmd, "SITE") == 0 || strcmp(cmd, "SYST") == 0 || strcmp(cmd, "STAT") == 0 || strcmp(cmd, "HELP") == 0){
    return OTHER_C;
  }

  return BAD_C;
}

//Used Wikipedia Berkeley(from HW link) TCP Socket Server code snippet as template code
int main(int argc, char** argv) {

  struct sockaddr_in sa;
  int SocketFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
  int thisPort;
  if (SocketFD == -1) {
    perror("cannot create socket");
    exit(EXIT_FAILURE);
  }

  memset(&sa, 0, sizeof sa);

  sa.sin_family = AF_INET;
  thisPort = strtol(argv[1], NULL, 10);
  //sa.sin_port = htons(1100);
  sa.sin_port = htons(thisPort);
  sa.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
  //sa.sin_addr.s_addr = inet_addr("127.0.0.1");
  
  if (bind(SocketFD,(struct sockaddr *)&sa, sizeof sa) == -1) {
    perror("bind failed");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }

  if (listen(SocketFD, 10) == -1) {
    perror("listen failed");
    close(SocketFD);
    exit(EXIT_FAILURE);
  }
  //socket data stuff
  struct sockaddr_in sa_data;
  int DataFD, bin;

  //read/write
  int fd, fd2;
  int w_temp, r_temp;
  struct stat fst;

  //cmd variables
  char cmd_buf[100];
  int cmdInt;
  char *cmd, *next;
  const char* respStr;
  const char* CWD = "/home/user/ftp/";

  //flags that keep track of respective states
  bool authFLG, quitFLG, dataFLG, typeFLG;

  //LIST, STOR, RETR
  int check, status, numBytesRead;
  bool flagRead;
  char exec_buf[1024], read_buf[1024];

  //IPaddr stuff
  int i, dataPort;
  const char* dummy;
  char IP_str[INET_ADDRSTRLEN];

  for (;;) {
    int ConnectFD = accept(SocketFD, NULL, NULL);
    authFLG = false;
    quitFLG = false;
    dataFLG = false;
    typeFLG = false;

    if (0 > ConnectFD) {
      perror("accept failed");
      close(SocketFD);
      exit(EXIT_FAILURE);
    }
    perror("incoming connection");

    respStr = "220 Service ready for new user.\r\n";
    memcpy(cmd_buf, respStr, strlen(respStr)+1);
    w_temp = write(ConnectFD, cmd_buf, strlen(respStr));

    for(;;){
      memset(cmd_buf, '\0', 100);

      r_temp = read(ConnectFD, cmd_buf, 99);
      perror(cmd_buf);

      cmd = strtok(strdup(cmd_buf), " \r\n");
      cmdInt = cmdToInt(cmd);

      switch(cmdInt){
      case QUIT_C: 
	respStr = "221 Service closing control connection.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	quitFLG = true;
	break;
      case USER_C:
	respStr = "230 User logged in, proceed.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	authFLG = true;
	break;
      case PORT_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	if(!dataFLG){
	  IP_str[0] = '\0';
	  for (i = 0; i < 4; i++){
	    dummy = strtok(NULL, ",");
	    strcat(IP_str, dummy);
	    if(i < 3)
	      strcat(IP_str, ".");
	  }
	  dataPort = strtol(strtok(NULL, ","), NULL, 10);
	  dataPort <<= 8;
	  dataPort += strtol(strtok(NULL, "\r\n"), NULL, 10);
	  DataFD = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	  if (DataFD == -1) {
	    perror("cannot create socket");
	    exit(EXIT_FAILURE);
	  }

	  memset(&sa_data, 0, sizeof sa_data);

	  sa_data.sin_family = AF_INET;
	  sa_data.sin_port = htons(dataPort);
	  bin = inet_pton(AF_INET, IP_str, &sa_data.sin_addr);

	  if (connect(DataFD, (struct sockaddr *)&sa_data, sizeof sa_data) == -1) {
	    perror("connect failed");
	    close(ConnectFD);
	    close(SocketFD);
	    close(DataFD);
	    exit(EXIT_FAILURE);
	  }
	  printf("socket connected");
	}
	dataFLG = true;		  
	respStr = "200 Command Successful.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));	  
	break;
      case TYPE_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	next = strtok(NULL, "\r\n");
	if(strcmp(next, "I") == 0) {
	  typeFLG = true;
	  respStr = "200 Command Successful.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	else if( (strcmp(next, "A") == 0) | (strcmp(next, "E") == 0) | (strcmp(next, "L") == 0) ){
	  respStr = "504 Command not implemented for that parameter.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	else {
	  respStr = "501 Syntax error in parameters or arguments.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	break;
      case MODE_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	next = strtok(NULL, "\r\n");
	if(strcmp(next, "S") == 0) {
	  respStr = "200 Command Successful.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	else if( (strcmp(next, "B") == 0) | (strcmp(next, "C") == 0) ){
	  respStr = "504 Command not implemented for that parameter.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	else {
	  respStr = "501 Syntax error in parameters or arguments.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	break;

      case STRU_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	next = strtok(NULL, "\r\n");
	if(strcmp(next, "F") == 0) {
	  respStr = "200 STRU Command Successful.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	else if( (strcmp(next, "R") == 0) | (strcmp(next, "P") == 0) ){
	  respStr = "504 Command not implemented for that parameter.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	else {
	  respStr = "501 Syntax error in parameters or arguments.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	}
	break;
      case STOR_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	if(!typeFLG){
	  respStr = "451 Requested action aborted: local error in processing.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	next = strtok(NULL, "\r\n");

	respStr = "125 Data connection already open; transfer starting.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));

	//memset(read_buf, 0, 1024);
	if(next != NULL){
	  strcat(read_buf, next);
	}
	fd = open(read_buf, O_RDWR | O_CREAT | O_TRUNC, 0600);
	flagRead = false;
	numBytesRead = 0;
	for(;;){
	  if((numBytesRead = recv(DataFD, next, 1024, 0)) < 0){
	    perror("error in reading\n");
	    break;
	  }
	  else if(numBytesRead < 1024){
	    flagRead = true;  
	  }
	  //fprintf(stderr, "reading %d bytes\n", numBytesRead);
	  w_temp = write(fd, next, numBytesRead);

	  if(flagRead)
	    break;
	}
	close(DataFD);
	close(fd);
	dataFLG = false;

	respStr = "226 Closing data connection. Requested file transfer successful.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	break;
      case RETR_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	if(!typeFLG){
	  respStr = "451 Requested action aborted: local error in processing.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	next = strtok(NULL, "\r\n");
	

	respStr = "125 Data connection already open; transfer starting.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));

	memset(read_buf, 0, 1024);
	if(next != NULL){
	  strcat(read_buf, CWD);
	  strcat(read_buf, next);
	}
	fd = open(read_buf, O_RDONLY);
	fstat(fd, &fst);
	check = sendfile(DataFD,fd, NULL, fst.st_size);
	close(DataFD);
	close(fd);
	dataFLG = false;

	respStr = "226 Closing data connection. Requested file transfer successful.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));	  
	break;
      case LIST_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	next = strtok(NULL, "\r\n");
	respStr = "125 Data connection already open; transfer starting.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));

	fd2 = dup(STDOUT_FILENO);
	dup2(DataFD, STDOUT_FILENO);

	memset(exec_buf, 0, 1024);
	
	strcat(exec_buf, "ls -l ");
	if(next != NULL){
	  strcat(exec_buf, CWD);
	  strcat(exec_buf, next);
	}

	status = system(exec_buf);

	close(DataFD);
	dataFLG = false;
	close(STDOUT_FILENO);
	dup2(fd2, STDOUT_FILENO);

	respStr = "226 Closing data connection. Requested file transfer successful.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	break;
      case NOOP_C:
	respStr = "200 NOOP Command Successful.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	break;
      case MKD_C:
	if(!authFLG){
	  respStr = "530 Not logged in.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	next = strtok(NULL, "\r\n");
	memset(exec_buf, 0, 1024);
	strcat(exec_buf, "mkdir ");
	if(strstr(next, "..") != NULL){
	  respStr = "501 Syntax error in parameters or arguments.\r\n";
	  memcpy(cmd_buf, respStr, strlen(respStr)+1);
	  w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	  break;
	}
	else if(next != NULL){
	  strcat(exec_buf, CWD);
	  strcat(exec_buf, next);
	}
	status = system(exec_buf);
	//mkdir(next, S_IRWXU);
	if(status != 0){
	  /*char* tempStr = "501 ";
	  strcat(tempStr, CWD);
	  strcat(tempStr, next);
	  strcat(tempStr," Exists.\r\n");*/
	  respStr = "501 Directory Exists\r\n";
	}
	else{
	  /*char* tempStr = "257 ";
	  strcat(tempStr, CWD);
	  strcat(tempStr, next);
	  strcat(tempStr," Created Successfully.\r\n");*/
	  respStr = "257 Directory Created Successfully\r\n";
	}
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	break;    
      case OTHER_C:
	respStr = "502 Command not implemented.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	break;
      default:
	respStr = "500 Syntax error, command unrecognized.\r\n";
	memcpy(cmd_buf, respStr, strlen(respStr)+1);
	w_temp = write(ConnectFD, cmd_buf, strlen(respStr));
	break;
      }
      if(quitFLG){
	break;
      }
    }

      perror("closing connection\n");

      if(dataFLG){
	shutdown(DataFD, SHUT_RDWR);
	close(DataFD);
      }

      if (shutdown(ConnectFD, SHUT_RDWR) == -1) {
	perror("shutdown failed");
	close(ConnectFD);
	close(SocketFD);
	exit(EXIT_FAILURE);
      }
      close(ConnectFD);
  }
  //end for(;;)
  
  /* if (argc < 2) {
       std::cerr << "Usage: " << argv[0] << " PORT-NUMBER" << std::endl;
       return 1;
       }
       const char *portname = argv[1];

       std::cout << "Code to bind to 127.0.0.1 port " << portname
       << " and listen for connections unimplemented." << std::endl;
       return 0; */
  
  close(SocketFD);
  if(w_temp < 0 && r_temp < 0 && status < 0 && bin < 0 && check < 0){
    perror("Status error\n");
  }
  return EXIT_SUCCESS;  
}
