// g++-9 -o client client.cpp

#include <iostream>
#include <cstring>
#include <arpa/inet.h>
#include <unistd.h>
#include <fstream>
#include <thread>
#include <chrono>
#include <vector>

const int PORT = 8080;
const std::string SERVER_IP = "127.0.0.1";

int get_ack(int socket) {
    char ack_buffer[1024] = {0};
    recv(socket, ack_buffer, sizeof(ack_buffer), 0);
    std::cout << "Server: " << ack_buffer << "\n";
    return 0;
}

int send_file(int socket, const std::string& file_path) {
    std::ifstream file_stream(file_path, std::ios::binary | std::ios::ate);
    if (!file_stream.is_open()) {
        std::cerr << "Error opening file: " << file_path << std::endl;
        return -1;
    }

    std::streamsize file_size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    // Send the file size first
    uint32_t size_to_send = htonl(file_size);
    send(socket, &size_to_send, sizeof(size_to_send), 0);

    if (get_ack(socket) != 0) {
        std::cerr << "Error receiving acknowledgement of " << file_path << " size.\n";
        return -1;
    }

    char buffer[1024];
    while (file_stream.read(buffer, sizeof(buffer))) {
        send(socket, buffer, file_stream.gcount(), 0);
    }
    if (file_stream.gcount() > 0) {
        send(socket, buffer, file_stream.gcount(), 0);
    }

    if (get_ack(socket) != 0) {
        std::cerr << "Error receiving acknowledgement of " << file_path << " file.\n";
        return -1;
    }
    file_stream.close();
    return 0;
}

void send_end_of_job(int socket) {
    uint32_t end_signal = 0;
    send(socket, &end_signal, sizeof(end_signal), 0);
    get_ack(socket);
}

bool ask_server_job_status(int socket) {
    std::string status_request = "CHECK_DONE";
    send(socket, status_request.c_str(), status_request.size(), 0);

    char response_buffer[1024] = {0};
    recv(socket, response_buffer, sizeof(response_buffer), 0);
    std::string response = response_buffer;

    std::cout << "Server response to CHECK_DONE: " << response << std::endl;

    if (response.find("Job ready.") != std::string::npos) {
        return true;
    } else {
        return false;
    }
}
int send_ack(int socket) {
    uint32_t ack = htonl(0); // ACK value (you can adjust if you have specific ACK values)
    ssize_t bytes_sent = send(socket, &ack, sizeof(ack), 0);
    if (bytes_sent != sizeof(ack)) {
        std::cerr << "Error sending ACK.\n";
        return -1;
    }
    std::cout << "Sending ack: " << ack << "\n";
    return 0;
}






void receive_done_wav(int socket) {
    // Tell server that you want file first
    if (send_ack(socket) != 0) {
        std::cerr << "Error sending acknowledgement for file size.\n";
        return;
    }
    // Receive the file size
    std::cout << "Receiving done.wav size...\n";
    uint32_t file_size;
    ssize_t bytes_received = recv(socket, &file_size, sizeof(file_size), 0);
    if (bytes_received != sizeof(file_size)) {
        std::cerr << "Error receiving done.wav size.\n";
        return;
    }
    std::cout << "Converting file size... " << std::endl;
    file_size = ntohl(file_size); // ensure proper conversion
    std::cout << "Received file size. " << file_size << std::endl;

    // Send acknowledgement for size
    if (send_ack(socket) != 0) {
        std::cerr << "Error sending acknowledgement for file size.\n";
        return;
    }

    std::ofstream done_wav("done.wav", std::ios::binary);
    char buffer[1024];
    size_t total_received = 0;
    while (total_received < file_size) {
        bytes_received = recv(socket, buffer, sizeof(buffer), 0);
        if (bytes_received <= 0) {
            if (bytes_received == 0) {
                std::cerr << "Connection closed prematurely while receiving done.wav.\n";
            } else {
                std::cerr << "Error receiving done.wav data.\n";
            }
            done_wav.close();
            std::remove("done.wav"); // Delete partially received file
            return;
        }
        done_wav.write(buffer, bytes_received);
        total_received += bytes_received;
    }
    done_wav.close();
    std::cout << "Received done.wav file.\n";

    // Send acknowledgement for file
    if (send_ack(socket) != 0) {
        std::cerr << "Error sending acknowledgement for done.wav file.\n";
        return;
    }
}



/*
int main() { //debug main
    int sock = 0;
    struct sockaddr_in serv_addr;
    int repeater;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        std::cerr << "Socket creation error" << std::endl;
        return -1;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(PORT);

    if (inet_pton(AF_INET, SERVER_IP.c_str(), &serv_addr.sin_addr) <= 0) {
        std::cerr << "Invalid address/ Address not supported" << std::endl;
        return -1;
    }

    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        std::cerr << "Connection Failed" << std::endl;
        return -1;
    }

    std::vector<std::string> wav_paths = {"./1/1/cymbals.wav", "./1/2/harp.wav"};
    std::vector<std::string> txt_paths = {"./1/1/instructions.txt", "./1/2/instructions.txt"};

    for (size_t i = 0; i < wav_paths.size(); ++i) {
        repeater = 1;
        while (repeater != 0) {
            std::cout << "Input wav: " << wav_paths[i] << std::endl;
            repeater = send_file(sock, wav_paths[i]);
        }

        repeater = 1;
        while (repeater != 0) {
            std::cout << "Input text: " << txt_paths[i] << std::endl;
            repeater = send_file(sock, txt_paths[i]);
        }

        std::string add_another = (i < wav_paths.size() - 1) ? "y" : "n";
        std::cout << "Add another sound? <y/n>: " << add_another << std::endl;
        if (add_another != "y") {
            send_end_of_job(sock);
            break;
        }
    }

    // Wait for the server to mark the job as ready
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(5));
        if (ask_server_job_status(sock)) {
            receive_done_wav(sock);
            break;
        } else {
            std::cout << "Server is still processing jobs. Waiting..." << std::endl;
        }
    }

    close(sock);
    return 0;
}

*/

int main() { // main
    int ask = 0;
    do {
        int sock = 0;
        struct sockaddr_in serv_addr;
        int repeater;

        if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
            std::cerr << "Socket creation error" << std::endl;
            return -1;
        }

        serv_addr.sin_family = AF_INET;
        serv_addr.sin_port = htons(PORT);

        if (inet_pton(AF_INET, SERVER_IP.c_str(), &serv_addr.sin_addr) <= 0) {
            std::cerr << "Invalid address/ Address not supported" << std::endl;
            return -1;
        }

        if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
            std::cerr << "Connection Failed" << std::endl;
            return -1;
        }

        while (true) {
            std::string wav_path, txt_path;

            repeater = 1;
            while (repeater != 0) {
                std::cout << "Input wav: ";
                std::getline(std::cin >> std::ws, wav_path); // Read input with handling whitespace
                repeater = send_file(sock, wav_path);
            }

            repeater = 1;
            while (repeater != 0) {
                std::cout << "Input text: ";
                std::getline(std::cin >> std::ws, txt_path); // Read input with handling whitespace
                repeater = send_file(sock, txt_path);
            }

            std::string add_another;
            std::cout << "Add another sound? <y/n>: ";
            std::getline(std::cin >> std::ws, add_another); // Read input with handling whitespace
            if (add_another != "y") {
                send_end_of_job(sock);
                break;
            }
        }

        // Wait for the server to mark the job as ready
        while (true) {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            if (ask_server_job_status(sock)) {
                receive_done_wav(sock);
                break;
            } else {
                std::cout << "Server is still processing jobs. Waiting..." << std::endl;
            }
        }

        close(sock);

        std::string nex;
        std::cout << "Process another song? <y/n>: ";
        std::getline(std::cin >> std::ws, nex); // Read input with handling whitespace
        if (nex != "y") {
            ask = 1;
        }

    } while (ask == 0);

    return 0;
}
