// **************************************************************************************
// * webServer (webServer.cpp)
// * - Implements a very limited subset of HTTP/1.0, use -v to enable verbose debugging output.
// * - Port number 1701 is the default, if in use random number is selected.
// *
// * - GET requests are processed, all other metods result in 400.
// *     All header gracefully ignored
// *     Files will only be served from cwd and must have format file\d.html or image\d.jpg
// *
// * - Response to a valid get for a legal filename
// *     status line (i.e., response method)
// *     Cotent-Length:
// *     Content-Type:
// *     \r\n
// *     requested file.
// *
// * - Response to a GET that contains a filename that does not exist or is not allowed
// *     statu line w/code 404 (not found)
// *
// * - CSCI 471 - All other requests return 400
// * - CSCI 598 - HEAD and POST must also be processed.
// *
// * - Program is terminated with SIGINT (ctrl-C)
// **************************************************************************************
#include "web_server.h"

int main(int argc, char *argv[]) {
    // ********************************************************************
    // * Process the command line arguments
    // ********************************************************************
    int opt = 0;
    while ((opt = getopt(argc, argv, "d:")) != -1) {
        switch (opt) {
            case 'd':
                LOG_LEVEL = std::stoi(optarg);
                break;
            case ':':
            case '?':
            default:
                std::cout << "useage: " << argv[0] << " -d LOG_LEVEL" << std::endl;
                exit(-1);
        }
    }


    // *******************************************************************
    // * Catch all possible signals
    // ********************************************************************
    //DEBUG << "Setting up signal handlers" << ENDL;


    // *******************************************************************
    // * Creating the inital socket using the socket() call.
    // ********************************************************************
    int listenFd = socket(PF_INET, SOCK_STREAM, 0);
    DEBUG << "Calling Socket() assigned file descriptor " << listenFd << ENDL;


    // ********************************************************************
    // * The bind() call takes a structure used to spefiy the details of the connection.
    // *
    // * struct sockaddr_in servaddr;
    // *
    // On a cient it contains the address of the server to connect to.
    // On the server it specifies which IP address and port to lisen for connections.
    // If you want to listen for connections on any IP address you use the
    // address INADDR_ANY
    // ********************************************************************

    // ********************************************************************
    // * Binding configures the socket with the parameters we have
    // * specified in the servaddr structure.  This step is implicit in
    // * the connect() call, but must be explicitly listed for servers.
    // *
    // * Don't forget to check to see if bind() fails because the port
    // * you picked is in use, and if the port is in use, pick a different one.
    // ********************************************************************
    uint16_t port = 1032;
    DEBUG << "Calling bind()" << ENDL;

    struct sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    while (bind(listenFd, reinterpret_cast<struct sockaddr *>(&server_addr), sizeof(struct sockaddr)) == -1) {
        ERROR << "Failed to bind to socket\n"
          << "trying new port" << ENDL;
        port += 17;
        server_addr.sin_port = htons(port);
    }

    std::cout << "Using port: " << port << std::endl;

    // ********************************************************************
    // * Setting the socket to the listening state is the second step
    // * needed to being accepting connections.  This creates a que for
    // * connections and starts the kernel listening for connections.
    // ********************************************************************
    DEBUG << "Calling listen()" << ENDL;

    if (listen(listenFd, 20) != 0) {
        ERROR << "Failed to listen to socket: " + errno << ENDL;
    }

    // ********************************************************************
    // * The accept call will sleep, waiting for a connection.  When
    // * a connection request comes in the accept() call creates a NEW
    // * socket with a new fd that will be used for the communication.
    // ********************************************************************
    int quitProgram = 0;

    while (!quitProgram) {
        char recv_buf[1024 * 1024] = {};

        char *data = (char *) malloc(1024 * 1024);
        memset(data, '\0', 1024 * 1024);
        std::byte *dataBytes;
        int length = 0;

        auto *successImage = new std::string(
            "HTTP/1.0 200 OK\r\nServer: SillyStupid/0.1\r\nContent-type: image/jpeg\r\nContent-Length: ");
        auto *success = new std::string(
            "HTTP/1.0 200 OK\r\nServer: SillyStupid/0.1\r\nContent-type: text/html; charset=utf-8\r\nContent-Length: ");
        char fnf[] =
                "HTTP/1.0 404 File Not Found\r\nServer: SillyStupid/0.1\r\nContent-type: text/html; charset=utf-8\r\nContent-Length: 23\r\n\r\n<h1>File Not Found</h1>";
        char badRequest[] =
                "HTTP/1.0 400 Bad Request\r\nServer: SillyStupid/0.1\r\nContent-type: application/json; charset=utf-8\r\nContent-Length: 79\r\n\r\n{\"error\": \"Bad request\",\"message\": \"Request body could not be read properly.\",}";

        int connFd = 0;
        bool found = false;
        DEBUG << "Calling connFd = accept(fd,NULL,NULL)." << ENDL;

        connFd = accept(listenFd, NULL,NULL);

        if (connFd > 0) {
            INFO << "Sucessfully connected" << ENDL;
        } else if (connFd < 0) {
            ERROR << "Failed to accept" << ENDL;
        }

        while (recv(connFd, recv_buf, sizeof(recv_buf), 0) > 0) {
            char *line = strtok(recv_buf, "\r\n");
            char *token = strtok(line, " ");
            DEBUG << token << ENDL;
            if (strcmp(token, "GET")) {
                send(connFd, badRequest, strlen(badRequest), 0);
                close(connFd);
                break;
            }
            token = strtok(NULL, " ");
            DEBUG << token << ENDL;

            char initpath[] = "./data";
            char *path = (char *) malloc(1024);
            memset(path, '\0', 1024);

            strcat(path, initpath);
            strcat(path, token);

            DEBUG << path << " " << strlen(path) << ENDL;

            if (!std::ifstream(path).good()) {
                send(connFd, fnf, sizeof(fnf), 0);
                close(connFd);
                break;
            }

            if (std::regex_match(token, std::regex("(/file[0-9]\\.html|/image[0-9]\\.jpg)"))) {
                found = true;
            } else {
                send(connFd, fnf, sizeof(fnf), 0);
                found = false;
                close(connFd);
                break;
            }

            if (std::regex_match(token, std::regex("(/file[0-9]\\.html)"))) {
                length = 0;
                std::ifstream *file = new std::ifstream(path, std::ios_base::in);
                file->read(data, 1024 * 1024);
                delete file;
            } else {
                FILE *fileptr = fopen(path, "rb");
                fseek(fileptr, 0, SEEK_END);
                length = ftell(fileptr);
                rewind(fileptr);
                dataBytes = (std::byte *) malloc(length * sizeof(char));
                memset(dataBytes, '\0', length * sizeof(char));
                fread(dataBytes, 1, length, fileptr);
                fclose(fileptr);
            }

            memset(recv_buf, '\0', strlen(recv_buf));
            break;
        }

        if (found) {
            std::string *response = new std::string("");

            if (length == 0) {
                response = new std::string(*success + std::to_string(strlen(data)) + "\r\n\r\n" + data);
            } else {
                response = new std::string(*successImage + std::to_string(length) + "\r\n\r\n");
            }

            DEBUG << response->size() << ENDL;

            send(connFd, response->data(), response->size(), 0);
            if (length != 0) {
                send(connFd, dataBytes, length, 0);
                memset(dataBytes, '\0', length);
            }
            memset(data, '\0', strlen(data));
            length = 0;
            delete response;
        } else {
            send(connFd, fnf, sizeof(fnf), 0);
        }

        DEBUG << "We have recieved a connection on "
              << connFd << ". Calling processConnection(" << connFd << ")" << ENDL;
        quitProgram = processConnection(connFd);
        DEBUG << "processConnection returned " << quitProgram << " (should always be 0)" << ENDL;
        DEBUG << "Closing file descriptor " << connFd << ENDL;
        close(connFd);
    }


    ERROR << "Program fell through to the end of main. A listening socket may have closed unexpectadly." << ENDL;
    closefrom(3);
    return 0;
}
