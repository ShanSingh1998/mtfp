#include "mftp.h"

struct sockaddr_in servAddr;
struct sockaddr_in servAddrData;
struct hostent* hostEntry;
struct in_addr** pptr;
int socket_fd, dataSocket;  //both data and network socket
int string_size = 500;  
int dataPort; //port for data connection
int DEBUG = 0; //Debug flag 

/* FUNCTIONS */
int clientRead(int clientFD, char *buffer); //Connect to the server through a socket
void DataConnectionPort();                  //Connect to server thorugh data socket
int GetData(char *message, char *buffer_read);  // Recieve data connection port number from server
void ExitProgram();

int main(int argc, char **argv){
    if(argc != 2 ){
        printf("<hostname | IP address>\n");
        exit(1);
    }

/* NORMAL CONNECTION */
    /* Making a connection from a client */   
    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(socket_fd < 0){
        perror("Socket Failed"), exit(1);
    }
    
    memset(&servAddr, 0 , sizeof(servAddr));
    servAddr.sin_family = AF_INET;
    servAddr.sin_port = htons(MY_PORT_NUMBER);
    
    hostEntry = gethostbyname(argv[1]);
    
    if (hostEntry){
        pptr = (struct in_addr**) hostEntry->h_addr_list;
        memcpy(&servAddr.sin_addr, *pptr, sizeof(struct in_addr));
    }

    else{
        herror(" Error with recieving host by name ");
        exit(1);
    }

    int connect_fd = connect(socket_fd, (struct sockaddr*)&servAddr, sizeof(servAddr));
    if (connect_fd < 0){ 
        perror("Connection Error"), exit(1);
    }

//======================================================================================

    printf("Connected to server %s\n", argv[1]);

    char *message = malloc(sizeof(char)*500);
    char *buffer_read = malloc(sizeof(char)*500);

    /* STRING/COMMAND PARSING */
    while(1){  
        printf("\nMFTP > ");
        char buffer[512];
        fgets(buffer, 512, stdin);
        buffer[strlen(buffer) - 1] = '\0';
        char copy[512], command[512], pathname[512];
        strcpy(copy, buffer);
        if(sscanf(copy, "%s %[^\n]", command, pathname) == 2){  //two commands saved into command and pathname
            int i;
            for(i = 0; command[i]; i++){
                command[i] = tolower(command[i]);   //convert command into all lowercase
            }
        }

        else if(sscanf(copy, "%[^\n]", command) == 1){ //Stores command from user input in stdin
            int i;
            for(i = 0; command[i]; i++){
                command[i] = tolower(command[i]);   
            }
        }

        if(strcmp("exit", command) == 0){   //Sends Q to the server and then exits
            strcpy(message, "Q");
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, "\n");
            if(DEBUG) printf("Message is: %s\n", message);     
            memset(buffer_read, 0, string_size);
            if (write(socket_fd, message, sizeof(char)*strlen(message)) < 0){   //Writing to command to socket 
                perror("Did not write to the server"), exit(1);
            } 
            clientRead(socket_fd, buffer_read); //Recieves A or E and that'll let us know if we can exit
            
            if (buffer_read[0] == 'E'){ //check if negative aknowledgement
                perror("\nServer Error ");
                printf("%s \n" , buffer_read+1);
            }
            free(buffer_read);
            free(message);
            ExitProgram();
        }

        if(strcmp("cd", command) == 0){ //Change directory of client 
            if(access(pathname, R_OK) != F_OK){
                printf("Not a directory");
                continue;
            }
            chdir(pathname);    
        }

        if(strcmp("rcd", command) == 0){ // Change directory of server
            strcpy(message, "C");
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, pathname);
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, "\n");
            memset(buffer_read, 0, string_size);
            if (write(socket_fd, message, sizeof(char)*strlen(message)) < 0){
                perror("Did not write to the server"), exit(1);
            } 
            clientRead(socket_fd, buffer_read);
            
            if (buffer_read[0] == 'E'){ //check if negative aknowledgement
                perror("\nServer Error ");
                printf("%s \n" , buffer_read+1);
            }

        }

        if(strcmp("ls", command) == 0){ //Lists all the contents of client directory
            if(fork()){
                wait(NULL);
                printf("Done\n");
            }

            else{    
                int fd[2];
                pipe (fd);                           //insert pipe     
                int rdr = fd[0], wtr = fd[1];
                fflush(stdout);
                        
                if (fork ()) { // Parent reads after child's done writing  
                    wait(NULL);  //waiting for process
                    close (wtr);    
                    dup2(rdr,0);    //Changes the stdin to the read end of the pipe
                    execlp("more", "more", "-20", NULL);       //Error check to see if argument cannot be executed 
                    perror("Exec failed right");
                }
                
                else {        // child becomes application and writes 
                    close(rdr);
                    close(0); fflush(stdout); dup2(wtr,1); //changes stdout to the pipe writer end
                    execlp("ls", "ls", "-l", NULL);         //Error check to see if argument cannot be executed 
                    perror("Exec failed left");      
                }
            } 
        }

        if (strcmp("rls", command) == 0){ //remote ls
            if (DEBUG) printf("RLS establish data connection\n"); 
            if(GetData(message, buffer_read) == 0){         //Recieves data connection port
                printf("failed getting a port number \n");
                continue;
            }
           
            DataConnectionPort();   //Creates data connection
            strcpy(message, "L\n"); 
            if (write(socket_fd, message, sizeof(char)*strlen(message)) < 0){ //Send L message to the server to do remote ls command
                    perror("Did not write to the server"), exit(1);  
                } 
            if (DEBUG) printf ("about to read from data connection \n");
            
            //read acknowledgement 
            clientRead(socket_fd, buffer_read);
            if (DEBUG) printf("server acknowledges: %s \n" , buffer_read);
            if (buffer_read[0] == 'E'){ //check if negative acknowledgement
                perror("server returned error");
                printf("%s \n" , buffer_read+1);
                close(dataSocket);
            }   

            if(DEBUG) printf("Buffer is:  %s\n", buffer_read);

            /*Process: reading from socket and writing to file*/
            int pid = fork();
            if(pid){    //parent
                close(dataSocket);
                wait(NULL);
            }
            else{   //child
                dup2(dataSocket, 0);
                execlp("more", "more", "-20", NULL);
                perror("more failed ");

            }
            close(dataSocket);
        }

        if(strcmp("get", command) == 0){    //Get file from server
            if (DEBUG) printf("GET establish data connection\n");
            if(GetData(message, buffer_read) == 0){
                printf("failed getting a port number \n");
                continue;
            }
           
            DataConnectionPort();
            strcpy(message, "G");
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, pathname);
            strcat(message, "\n");
            memset(buffer_read, 0, string_size);
            if(write(socket_fd, message, sizeof(char)*strlen(message)) < 0){
                perror("Did not write to the server");
                close(dataSocket);
                exit(1);  
            } 

            if (DEBUG) printf ("about to read from data connection \n");
            
            //read aknowledgement 
            clientRead(socket_fd, buffer_read);
            if (DEBUG) printf("Server Acknowledges: %s \n" , buffer_read);
            if (buffer_read[0] == 'E'){ //check if negative aknowledgement
                perror("Server Error ");
                printf("%s \n" , buffer_read+1);
                close(dataSocket);
                continue;
            }   

            if(DEBUG) printf("Buffer is:  %s\n", buffer_read);

            /*  Process: reading from socket and writing to file  */
            char buff[128];
            if(DEBUG) printf("Opening file\n");

            /* opening file for writing */
            int fp, n, slash = 0;

            if ((fp = open(pathname, O_WRONLY | O_CREAT | O_TRUNC)) < 0){
                perror("Cannot open output file\n"); break;
            }

            while(1){
                n = read(dataSocket, buff, 128);
                if (n <= 0) break;
                write(fp, buff, n);
                if(DEBUG) printf("Buffer is %s\n", buff);
            }
            
            close(fp);        
            close(dataSocket);
            if(DEBUG) printf("Finished writing\n");
        }
    
        if(strcmp("show", command) == 0){ //Print the contents of file in server
            if (DEBUG) printf("SHOW establish data connection\n");
            if(GetData(message, buffer_read) == 0){
                printf("failed getting a port number \n");
                continue;
            }
            DataConnectionPort();
            
            strcpy(message, "G");
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, pathname);
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, "\n");
            memset(buffer_read, 0, string_size);
            
            if(write(socket_fd, message, sizeof(char)*strlen(message)) < 0){
                perror("Did not write to the server");
                close(dataSocket); 
            } 

            if (DEBUG) printf ("about to read from data connection \n");
            //read acknowledgement 
            clientRead(socket_fd, buffer_read);
            if (DEBUG) printf("server acknowledges: %s \n" , buffer_read);
            if (buffer_read[0] == 'E'){ //check if negative acknowledgement
                perror("Server Error ");
                printf("%s \n" , buffer_read+1);
                close(dataSocket);
            } 

            if(DEBUG) printf("Buffer is: %s\n", buffer_read);

            /*Process: reading from socket and outputting to stdout 20 lines at a time*/
            char* buff = malloc(sizeof(char) * 128);
            if(DEBUG) printf("Opening file\n");

            /* opening file for writing */
            int fp, n;

            if (fork()){
                close(dataSocket);
                wait(NULL);
            }

            else{
                dup2(dataSocket,0);
                execlp("more", "more", "-20", NULL);
                perror("More failed ");
            }
            
            close(dataSocket);
            if(DEBUG) printf("Finished writing\n");
            free(buff);
        }
        
        if(strcmp("put", command) == 0){    // Put file from client to server
            int fp, n, slash = 0;
           
            if(DEBUG) printf("Path = %s\n", pathname);
            if ((fp = open(pathname, O_RDONLY, 644)) < 0){
                perror("Cannot open output file"); 
                continue;
            }

            if (DEBUG) printf("PUT establish data connection\n");
            if(GetData(message, buffer_read) == 0){
                printf("failed getting a port number \n");
                continue;
            }

            DataConnectionPort();
  
            strcpy(message, "P");
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, pathname);
            if(DEBUG) printf("Message is: %s\n", message);
            strcat(message, "\n");
            memset(buffer_read, 0, string_size);
            
            if(write(socket_fd, message, sizeof(char)*strlen(message)) < 0){
                perror("Did not write to the server");
                close(dataSocket); 
            } 

            if (DEBUG) printf ("about to read from data connection \n");
            //read acknowledgement 
            clientRead(socket_fd, buffer_read);
            if (DEBUG) printf("server acknowledges: %s \n" , buffer_read);
            if (buffer_read[0] == 'E'){ //check if negative acknowledgement
                perror("Server Error");
                printf("%s \n" , buffer_read+1);
                close(dataSocket);
                continue;
            } 

            if(DEBUG) printf("Buffer is: %s\n", buffer_read);

            /*Process: writing to socket after parsing through file*/
            char* buff = malloc(sizeof(char) * 128);

            if(DEBUG) printf("Opening file\n");

            /* opening file for writing */

            while(1){
                n = read(fp, buff, 128);
                if (n <= 0) break;
                write(dataSocket, buff, n);
                if(DEBUG) printf("Buffer is %s\n", buff);
            }
            
            free(buff);
            close(fp);        
            close(dataSocket);
            if(DEBUG) printf("Finished writing\n");
        }
        
        memset(buffer, 0, string_size);
    }
    return 0;
}
//=====================================================
//=====================================================

void ExitProgram(){
    //close(socket_fd);
    exit(0);
}

//=====================================================
//=====================================================

int clientRead(int clientFD, char *buffer){
    memset(buffer, 0, string_size);
    int n;
    while (1){
        n = read(clientFD, buffer, 1);
        if (n < 0){ 
            perror ("failed reading, source error"); exit(1);
        }
       
        if (n == 0) return -1;
        if (*buffer == '\n') break;
            buffer++;
    }
    strcat(buffer, "\0");
    return n;
}

//====================================================================
//===================================================================

void DataConnectionPort(){
/* DATA CONNECTION */
    dataSocket = socket(AF_INET, SOCK_STREAM, 0);
    if(dataSocket < 0){
        perror("Socket Failed"), exit(1);
    }

    memset(&servAddrData, 0 , sizeof(servAddr));
    servAddrData.sin_family = AF_INET;
    servAddrData.sin_port = htons(dataPort);
    if (hostEntry){
        pptr = (struct in_addr**) hostEntry ->h_addr_list;
        memcpy(&servAddrData.sin_addr, *pptr, sizeof(struct in_addr));
    }
    
    else{
        perror(" Error get host by name ");
        exit(1);
    }
    
    int connectStatus = connect(dataSocket, (struct sockaddr*)&servAddrData, sizeof(servAddr));
    if (connectStatus < 0) perror("Connection Error"), exit(1);
    if(DEBUG) printf("Data connection completed!\n");
}

//=============================================================================================
//=============================================================================================

int GetData(char *message, char *buffer_read){
    strcpy(message, "D\n");
    dataPort = 0;

    if (write(socket_fd, message, sizeof(char)*strlen(message)) < 0){
        perror("Did not write to the server");
        return 0;
    }   

    clientRead(socket_fd, buffer_read);
    if(DEBUG) printf("buffer = %s\n", buffer_read);

    int i = 0;
    for(i = 0; buffer_read[i] != '\0' ; i++){
        buffer_read[i] = buffer_read[i+1];
    }

    dataPort = atoi(buffer_read);
    if(DEBUG) printf("dataPort = %d\n", dataPort);
    return 1;
}