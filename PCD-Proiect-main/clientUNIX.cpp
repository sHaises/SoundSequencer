#include <iostream>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#include <cstring>

const char *SOCKET_PATH = "/tmp/job_server_socket";
const int BUFFER_SIZE = 1024;

void send_command(int sock, const std::string& command) {
    send(sock, command.c_str(), command.size(), 0);
}

std::string receive_response(int sock) {
    char buffer[BUFFER_SIZE] = {0};
    int valread = read(sock, buffer, BUFFER_SIZE);
    return std::string(buffer, valread);
}

int main() {
    int sock = 0;
    struct sockaddr_un server_addr;
    char buffer[BUFFER_SIZE] = {0};

    if ((sock = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error\n";
        return -1;
    }

    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "Connection failed\n";
        close(sock);
        return -1;
    }

    std::cout << "Connected to job server\n";

    while (true) {
        std::string command;
        std::cout << "Enter command (LIST, RESTART, EXIT): ";
        std::cin >> command;

        send_command(sock, command);

        if (command == "EXIT") {
            break;
        }

        std::string response = receive_response(sock);
        std::cout << "Server response: " << response << "\n";
    }

    close(sock);
    return 0;
}
