#include <sys/socket.h>
#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <errno.h>
#include <assert.h>
#include <netinet/in.h>
#include <unistd.h>
#include <signal.h>

/* This is marked volatile because Im using it as a constant check in a while loop*/
volatile sig_atomic_t terminate = 0;

void sigint_handler(int signum) {
    terminate = 1;
}

/* Called when the server shutsdown */
void finalize_exit(uint16_t* pcc_total){

    for(char i = 0; i < (126 - 32 + 1); i++){
        printf("char '%c' : %hu times\n", i + 32, pcc_total[(int) i]);
    }
    free(pcc_total);
    exit(0);
}

/* A macro to check if a character is within the printable range */
# define IS_PRINTABLE(x) ((x) >= 32 && (x) <= 126)

int main(int argc, char** argv){

    assert(argc == 2);

    struct sockaddr_in serv_addr;
    struct sockaddr_in peer_addr;

    // OBTAINING PORT IN HOST ENDIAN REP
    uint16_t rawport = (uint16_t) atoi(argv[1]);
    socklen_t addrsize = sizeof(struct sockaddr_in);


    // DEFINING THE SERVER CONFIG
    memset(&serv_addr, 0, sizeof(struct sockaddr_in));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(rawport);
    serv_addr.sin_addr.s_addr = htonl(INADDR_ANY);

    // obtaining socket fd
    int listener = socket(AF_INET, SOCK_STREAM, 0);

    // Set the SO_REUSEADDR option
    int optval = 1;
    if (setsockopt(listener, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval)) == -1) {
        perror("setsockopt\n");
        exit(EXIT_FAILURE);
    }

    // binding server to port
    if(bind(listener, (struct sockaddr*)&serv_addr, addrsize) != 0){
        perror("bind failed\n");
        return 1;
    }

    // listen on port with a queue of size 10
    if(listen(listener, 10) != 0){
        perror("listen failed\n");
        return 1;
    }

    /*
        Explanation for TA:
        im initializing an array that is of the same size as all
        the printable characters, to access how many times a printable
        character has been counter we can use:

        DS[C - 32] where C is the printable character 
    */
   unsigned char numreadable = 126 - 32 + 1;
   uint16_t* pcc_total = (uint16_t*) calloc(numreadable, sizeof(uint16_t));

   // assiging the signal handler to SIGINT
   signal(SIGINT, sigint_handler);


    // Server main logic
    while (!terminate){

        int connfd = accept(listener, (struct sockaddr*)&peer_addr, &addrsize);
        if (connfd < 0){
            if(errno == EINTR){
                finalize_exit(pcc_total);
            }
            continue;
        }

        // bytes_cnt is gonna act as a buffer that will store N - the number of bytes to be sent
        uint16_t bytes_cnt;

        // will store the received network byte
        unsigned char netn = 0;

        // amount of read bytes from read syscall
        ssize_t nread;

        // a flag to detect error that arent signal interrupts.
        char err_flag = 0;

        // read the first byte of the 16 bit N number sent by the client
        while ((nread = read(connfd, &netn, 1)) <= 0)
        {
            // if the read was not interrupted an error occured then connection error occured.
            if(nread == 0){
                err_flag = 1;
                break;
            }
            if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){
                err_flag = 1;
                break;
            }
            continue;

        }

        // terminate connection and proceed to next client
        if(err_flag){
            perror("connection error \n");
            close(connfd);
            continue;
        }

        /*
        since the first byte is the MSB byte because of network endianness
        we add it to bytes_cnt with a left shift by 8
        */
        bytes_cnt = netn << 8;

        /* Same as the first byte read but we add it to bytes_cnt as the LSB byte*/
        while ((nread = read(connfd, &netn, 1)) <= 0)
        {
            if(nread == 0){
                err_flag = 1;
                break;
            }
            if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){
                err_flag = 1;
                break;
            }
            continue;

        }

        if(err_flag){
            perror("connection error\n");
            close(connfd);
        }

        bytes_cnt += netn;


        /* Obtained the number of bytes to be sent now we keep reading user sent bytes to buffer until bytes_cnt bytes have been read*/

        // bytes_read is the amount of bytes we have read so far | bytes_cnt - bytes_read is the amount of bytes yet to read
        size_t bytes_read = 0;
        char* buffer = (char*) calloc(bytes_cnt, sizeof(char));

        // since we dont want to update pcc_total if client exited or something unexpected happened we store results in a temporary DS
        unsigned short* pcc_total_local = calloc(numreadable, sizeof(unsigned short));

        if(buffer == NULL || pcc_total_local == NULL){
            perror("failed buffer allocation\n");
            close(connfd);
            if(buffer) free(buffer);
            if(pcc_total_local) free(pcc_total_local);
            continue;
        }

        // keep track of printable chars to return back to user
        size_t printables = 0;

        while(bytes_read != bytes_cnt){
            while((nread = read(connfd, buffer, bytes_cnt - bytes_read)) <= 0){
                if(nread == 0){
                    err_flag = 1;
                    break;
                }
                if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){
                    err_flag = 1;
                    break;
                }
                continue;
            }

            if(err_flag){
                break;
            }


            for(int i = 0; i < nread; i++){
                if(!IS_PRINTABLE(buffer[i])){
                    continue;
                }
                printables += 1;
                pcc_total_local[buffer[i] - 32] += 1;
            }
            bytes_read += nread;
        }

        if(bytes_read != bytes_cnt){
            perror("connection error\n");
            free(pcc_total_local);
            free(buffer);
            close(connfd);
            continue;
        }


        // aux var to store amount of written bytes in write syscall
        ssize_t written = 0;
        // printables but in big endian
        uint16_t networkprintables = htons((uint16_t) printables);

        while((written = write(connfd, &networkprintables, sizeof(uint16_t))) <= 0){
            if(written == 0){
                err_flag = 1;
                break;
            }
            if(errno == ETIMEDOUT || errno == ECONNRESET || errno == EPIPE){
                err_flag = 1;
                break;
            }
            continue;
        }

        if(err_flag){
            perror("\nconnection error\n");
        }

        // After reading all bytes, register the results to main DS
        for(int i = 0; i < numreadable; i++){
            pcc_total[i] += pcc_total_local[i];
        }

        free(pcc_total_local);
        free(buffer);
        close(connfd);
    }
   
   finalize_exit(pcc_total);
}