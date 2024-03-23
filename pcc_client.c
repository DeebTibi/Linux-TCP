#include <arpa/inet.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>


int main(int argc, char** argv){
    if(argc != 4){
        perror("invalid argument count\n");
        return 1;
    }
    struct in_addr serveraddr;
    if (inet_pton(AF_INET, argv[1], &serveraddr) <= 0) {
        perror("inet_pton");
        return 1;
    }
    uint16_t port = (uint16_t)atoi(argv[2]);
    char* filepath = argv[3];

    // FILE* file = fopen(filepath, "r");
    // if (file == NULL) {
    //     perror("Error opening file");
    //     return 1;
    // }
    // fseek(file, 0L, SEEK_END);
    // uint16_t N = (uint16_t) ftell(file);
    // if (N == -1) {
    //     perror("Error getting file size");
    //     fclose(file);
    //     return 1;
    // }
    // rewind(file);

    int fd = open(filepath, O_RDONLY);
    if(fd == -1){
        perror("unable to open file\n");
        return 1;
    }

    int filesize = (int) lseek(fd, 0, SEEK_END);
    if (filesize == -1) {
        perror("lseek");
        close(fd);
        exit(EXIT_FAILURE);
    }
    if (lseek(fd, 0, SEEK_SET) == -1) {
        perror("lseek");
        close(fd);
        exit(EXIT_FAILURE);
    }
    uint16_t N = (uint16_t) filesize;
    
    
    struct sockaddr_in serversocket;
    int socketfd;
    if ((socketfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Error : Could not create socket \n");
        return 1;
    }
    serversocket.sin_family = AF_INET;
    serversocket.sin_addr = serveraddr;
    serversocket.sin_port = htons(port);

    if (connect(socketfd, (struct sockaddr *)&serversocket, sizeof(serversocket)) < 0) {
        perror("Error : Connect Failed. %s \n");
        return 1;
    }

    uint16_t networkN = htons(N);
    if(write(socketfd, &networkN, sizeof(uint16_t)) <= 0){
        perror("Error: write to server failed\n");
        return 1;
    }

    char filebuffer[1024];
    memset(filebuffer, 0, 1024);

    ssize_t file_read = 0;
    while((file_read = read(fd, filebuffer, 1024)) > 0){
        size_t total_bytes_sent = 0;
        ssize_t bytes_sent = 0;
        while((bytes_sent = write(socketfd, filebuffer + total_bytes_sent, file_read - total_bytes_sent)) > 0){
            total_bytes_sent += bytes_sent;
        }
        if (total_bytes_sent < file_read){
            perror("Error: could not write whole file to server\n");
            return 1;
        }
    }
    if (file_read < 0){
        perror("Error: could not read file to buffer\n");
        return 1;
    }
    uint16_t printable_chars = 0;
    if(read(socketfd, &printable_chars, sizeof(uint16_t)) <= 0){
        perror("Error: server closed connection or network error");
        return 1;
    }
    printable_chars = ntohs(printable_chars);
    printf("# of printable characters: %hu\n", printable_chars);
    return 0;
}