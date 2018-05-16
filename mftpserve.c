#include "mftp.h"

int listen_fd, connect_fd;
int dataPort, dataSocket, data_fd;
struct sockaddr_in servAdder;
struct sockaddr_in clientAddr;
struct hostent* hostEntry;

int DEBUG = 0;

void removeNewLineCharacter(char* buf);
int socket_create(int *listen_fd);
int bind_socket(struct sockaddr_in *servAddr);
int acceptConnection(struct sockaddr_in *clientAdder); 
char* getHost(struct sockaddr_in *clientAdder); //Recieves name of client
int serverRead(int clientFD, char *buffer); //Reads the command sent by the client 
void establishDataConnection(); //Generates a data connection number sends to client 

int main(int argc, char const *argv[]){
    socket_create(&listen_fd);
    
    bind_socket(&servAdder);

    if (listen(listen_fd, 4) < 0){
        perror("Listen failed"), exit(1);
    }
    
    while(true){
        connect_fd = acceptConnection(&clientAddr);
         
        if (fork()){
            close(connect_fd);
            waitpid(-1, NULL, WNOHANG);
        } 
        else{
            if(connect_fd < 0){
                perror("Accept failed"), exit(-1);
            }

            else{
                char *clientHostName = getHost(&clientAddr);
                printf("Connection Successful! Client name: %s\n",clientHostName);
                char buffer_read[512], buffer_write[512], command[512], pathname[512];
                char letter_read;

                if (DEBUG) printf("About to enter loop\n");
                while(1){
                    memset(buffer_read, 0, 512);
                    letter_read = '\0';
                    memset(command, 0, 512);
                    memset(buffer_write, 0, 512);
                    memset(pathname, 0, 512);

                    serverRead(connect_fd, command);
                    letter_read = command[0];

                    if(DEBUG) printf("Command is: %s\n", command);
                    if(DEBUG) printf("Letter is: %c\n", letter_read);
                    
                    if(strcmp("Q\n", command) == 0){ //Reads a Q from the client and exits
                        if(DEBUG) printf("Quitting\n");
                        strcpy(buffer_write, "A\n");
                        if(DEBUG) printf("Acknowledgement %s", buffer_write);

                        if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                            perror("Did not write to the server"), exit(1);  
                        } 

                        printf("Child %d has died\n", getpid());
                        close(connect_fd);
                        exit(0);
                    }
    
                    if(strcmp("D\n", command) == 0){    //Reads a D from the client and establishes a data connection
                        if(DEBUG) printf("Data port is starting\n");
                        establishDataConnection(&servAdder);
                    }

                    if(strcmp("L\n", command) == 0){   //This lists all the contents inside the server directory
                        strcpy(buffer_write, "A\n");
                        if (DEBUG) printf("Acknowledgement %s", buffer_write);

                        if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                            perror("Did not write to the server"), exit(1);  
                        }

                        int pid = fork();
                        if(pid){    //parent
                            close(data_fd);
                            wait(NULL);
                        }

                        else{   //child
                            dup2(data_fd, 1);
                            execlp("ls", "ls", "-l", NULL);
                            perror("ls failed");
                        }
                    }
        
                    if(letter_read == 'C'){ //This reads the RCD Command which changes server directory
                        if(DEBUG) printf("RCD\n");
                        removeNewLineCharacter(command);

                        strcpy(pathname, (command+1));
                        if(DEBUG) printf("Pathname: %s\n", pathname);
                        //Recieve absolute pathname and parse to get last file name
                        if((access(pathname, R_OK)) != F_OK){
                            strcpy(buffer_write, "E: Pathname doesn't exist\n");
                            if(DEBUG) printf("Acknowledgement %s", buffer_write);

                            if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                                perror("Did not write to the server"), exit(1);  
                            } 
                            
                            continue;
                        }
 
                        if(DEBUG) printf("Changing Directory\n");
                        strcpy(buffer_write, "A\n");
                        if(DEBUG) printf("Acknowledgement %s", buffer_write);

                        if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                            perror("Did not write to the server"), exit(1);  
                        } 
                        chdir(pathname); 
                    }

                    if(letter_read == 'G'){ 
                        int fp, n, slash = 0;
                        if(DEBUG) printf("Entering Get or show\n");
        
                        removeNewLineCharacter(command); 
                        if(DEBUG) printf("Command = %s\n", command);               
                        
                        strcpy(pathname, (command+1));

                        if(DEBUG) printf("Slash Pathname: %s\n", pathname);
                      
                        if ((fp = open(pathname, O_RDONLY, 644)) < 0){
                            strcpy(buffer_write, "E: File doesn't Exist\n");
                            if(DEBUG) printf("Acknowledgement %s", buffer_write);

                            if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                                perror("Did not write to the server"), exit(1);  
                            }
                             
                            continue;
                        }

                        strcpy(buffer_write, "A\n");
                        if (DEBUG) printf("Acknowledgement %s", buffer_write);

                        if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                            perror("Did not write to the server"), exit(1);  
                        }

                        /*Process: writing to socket after parsing through file*/
                        char* buff = malloc(sizeof(char) * 128);

                        if(DEBUG) printf("Writing file\n");

                        /* opening file for writing */

                        while(1){
                            n = read(fp, buff, 128);
                            if (n <= 0) break;
                            write(data_fd, buff, n);
                            //if(DEBUG) printf("Buffer is %s\n", buff);
                        }
                        
                        free(buff);
                        close(fp);        
                        close(data_fd);
                        //if(DEBUG) printf("Finished writing\n");
                    }

                    if(letter_read == 'P'){
                        if(DEBUG) printf("Entering Put\n");
        
                        removeNewLineCharacter(command); 
                        if(DEBUG) printf("Command = %s\n", command);               
                        
                        strcpy(pathname, (command+1));
                        if(DEBUG) printf("Slash Pathname: %s\n", pathname);
                             

                       /*Process: reading from socket and writing to file*/
                        char buff[128];
                        if(DEBUG) printf("Opening file\n");

                        /* opening file for writing */
                        int fp, n, slash = 0;

                        if ((fp = open(pathname, O_WRONLY | O_CREAT | O_TRUNC)) < 0){
                            strcpy(buffer_write, "E: File doesn't Exist\n");
                            if(DEBUG) printf("Acknowledgement %s", buffer_write);

                            if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                                perror("Did not write to the server"), exit(1);  
                            }
                            continue;
                        }

                        strcpy(buffer_write, "A\n");
                        if (DEBUG) printf("Acknowledgement %s", buffer_write);

                        if (write(connect_fd, buffer_write, sizeof(char)*strlen(buffer_write)) < 0){ //Sending Acknowledgement
                            perror("Did not write to the server"), exit(1);  
                        }

                        while(1){
                            n = read(data_fd, buff, 128);
                            if (n <= 0) break;
                            write(fp, buff, n);
                            //if(DEBUG) printf("Buffer is %s\n", buff);
                        } 
                        close(fp);        
                        close(data_fd);
                        if(DEBUG) printf("Finished writing\n");
                    }
                }    
            }
        }           
    }
    return 0;
}

/* Creating socket file descriptor */
int socket_create(int *listen_fd){
    *listen_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(listen_fd < 0){
            perror("Socket Failed"), exit(1);
        }
        return 0;
}

/* Forcefully attaching socket to the port 49999 */
int bind_socket(struct sockaddr_in *servAddr){
    memset(servAddr, 0, sizeof(servAdder));
    servAddr -> sin_family = AF_INET;
    servAddr -> sin_port = htons(MY_PORT_NUMBER);
    servAddr -> sin_addr.s_addr = htonl(INADDR_ANY);
 
    if (bind(listen_fd, (struct sockaddr *) servAddr, sizeof(servAdder)) < 0) {
        perror("Bind Failed"), exit(-1);
    }
    return 0;
}

//==============================================================================
//==============================================================================

/* Accept connections */
int acceptConnection(struct sockaddr_in *clientAdder){
    int length = sizeof(struct sockaddr_in);
 
    connect_fd = accept(listen_fd, (struct sockaddr *) clientAdder, &length);
    if (connect_fd < 0){
        perror("Accept Failed"), exit(1);
    }
    return connect_fd;
}

//===============================================================================
//===============================================================================

/* Getting a text host name */
char* getHost(struct sockaddr_in *clientAdder){
    char* hostName;
    hostEntry = gethostbyaddr(&(clientAdder->sin_addr), sizeof(struct in_addr), AF_INET);
    if (hostEntry == NULL){
        hostName = "Host is unknown";
    }
    
    else{
    hostName = hostEntry->h_name;
    }

    return hostName;
}

//===============================================================================
//===============================================================================

int serverRead(int clientFD, char *buffer){
    memset(buffer, 0, 500);
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

//=============================================================================
//=============================================================================

void establishDataConnection(struct sockaddr_in *servAddr){
    socket_create(&dataSocket);
    memset(servAddr, 0, sizeof(servAdder));
    servAddr -> sin_family = AF_INET;
    servAddr -> sin_port = htons(0);
    servAddr -> sin_addr.s_addr = htonl(INADDR_ANY);
 
    if (bind(dataSocket, (struct sockaddr *) servAddr, sizeof(servAdder)) < 0) {
        perror("Bind Failed"), exit(-1);
    }

    if (listen(dataSocket, 1) < 0){
        perror("Listen failed"), exit(1);
    }


    int length = sizeof(struct sockaddr_in);
    struct sockaddr_in emptyAddr;
    int status = getsockname(dataSocket,(struct sockaddr*) &emptyAddr,(socklen_t*) &length);
    if (status < 0 ){
        perror("Getsockname error");
    }

    dataPort = ntohs(emptyAddr.sin_port);
    if (DEBUG) printf("Data port: %d\n", dataPort);
    char buffer[50], buffer2[50];
    strcpy(buffer, "A");
    sprintf(buffer+1,"%d\n", dataPort);
    if(DEBUG) printf("Buffer is %s\n", buffer);
    
    if (write(connect_fd, buffer, sizeof(char)*strlen(buffer)) < 0){ //Sending Acknowledgement
        perror("Did not write to the server"), exit(1);  
    }

    data_fd = accept(dataSocket, (struct sockaddr*) &clientAddr, (socklen_t*) &length);
    printf("Child %d | Data Connection Port: %d\n", getpid(), dataPort);
}



//====================================================================
//==================================================================


void removeNewLineCharacter(char* buf){
    int i = 0;
    while(buf[i]){
        if (buf[i] == '\n'){
        buf[i] = '\0';
        break;
    }

    i++;
    
    }
}
