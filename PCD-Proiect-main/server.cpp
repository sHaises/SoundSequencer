// g++-9 -o server server.cpp -pthread

#include <iostream>
#include <thread>
#include <vector>
#include <cstring>
#include <netinet/in.h>
#include <unistd.h>
#include <fstream>
#include <sstream>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <dirent.h>
#include <mutex>
#include <condition_variable>
#include <atomic>
#include <chrono>

const int PORT = 8080;
const int MAX_CLIENTS = 10;
const std::string SERVER_FOLDER = "./jobs/";
const std::string WORKER_EXEC = "./worker";
const uint32_t MAX_FILE_SIZE = 100 * 1024 * 1024; // 100 MB limit

std::mutex folder_mutex;
std::atomic<bool> is_wav_expected(true);

void send_ack(int sock, const std::string& message) {
    send(sock, message.c_str(), message.size(), 0);
}

bool is_directory(const std::string& path) {
    DIR* directory = opendir(path.c_str());
    if (directory != nullptr) {
        closedir(directory);
        return true;
    }
    return false;
}

std::vector<std::string> get_directories(const std::string& path) {
    std::vector<std::string> directories;
    DIR* directory = opendir(path.c_str());
    if (directory != nullptr) {
        dirent* entry;
        while ((entry = readdir(directory)) != nullptr) {
            if (entry->d_type == DT_DIR && std::string(entry->d_name) != "." && std::string(entry->d_name) != "..") {
                directories.push_back(entry->d_name);
            }
        }
        closedir(directory);
    }
    return directories;
}

int get_ack(int socket) {
    char ack_buffer[1024] = {0};
    recv(socket, ack_buffer, sizeof(ack_buffer), 0);
    std::cout << "Server ack: " << ack_buffer << "\n";
    return 0;
}

int get_ack1(int socket) {
    uint32_t ack;
    ssize_t bytes_received = recv(socket, &ack, sizeof(ack), 0);
    if (bytes_received != sizeof(ack)) {
        std::cerr << "Error receiving ACK.\n";
        return -1;
    }
    ack = ntohl(ack); // convert ACK value if necessary
    std::cout << "Server ack: " << ack << "\n";
    return ack; // return the received ACK value or handle as needed
}



// Function to send a file over the socket
int send_file(int socket, const std::string& file_path) {
    
    std::cout << "Ready to send.\n";

    if (get_ack1(socket) != 0) {
        std::cerr << "Error receiving acknowledgement of send start\n";
        return -1;
    }
    std::cout << "Starting sending...\n";

    
    std::ifstream file_stream(file_path, std::ios::binary | std::ios::ate);
    if (!file_stream.is_open()) {
        std::cerr << "Error opening file: " << file_path << std::endl;
        return -1;
    }

    std::streamsize file_size = file_stream.tellg();
    file_stream.seekg(0, std::ios::beg);

    // Send the file size first
    uint32_t size_to_send = htonl(static_cast<uint32_t>(file_size)); // ensure proper conversion
    std::cout << "File size to send: " << file_size << " bytes\n";
    if (send(socket, &size_to_send, sizeof(size_to_send), 0) == -1) {
        std::cerr << "Error sending file size.\n";
        return -1;
    }
    std::cout << "Sent file size.\n";

    if (get_ack1(socket) != 0) {
        std::cerr << "Error receiving acknowledgement of " << file_path << " size.\n";
        return -1;
    }
    std::cout << "Received acknowledgment of file size.\n";

    char buffer[1024];
    while (file_stream.read(buffer, sizeof(buffer))) {
        ssize_t bytes_sent = send(socket, buffer, file_stream.gcount(), 0);
        if (bytes_sent == -1) {
            std::cerr << "Error sending file data.\n";
            return -1;
        }
        std::cout << "Sent " << bytes_sent << " bytes of file data.\n";
    }
    if (file_stream.gcount() > 0) {
        ssize_t bytes_sent = send(socket, buffer, file_stream.gcount(), 0);
        if (bytes_sent == -1) {
            std::cerr << "Error sending file data.\n";
            return -1;
        }
        std::cout << "Sent last " << bytes_sent << " bytes of file data.\n";
    }

    if (get_ack1(socket) != 0) {
        std::cerr << "Error receiving acknowledgement of " << file_path << " file.\n";
        return -1;
    }
    std::cout << "Received acknowledgment of file data.\n";

    file_stream.close();
    std::cout << "Closed file stream.\n";

    return 0;
}



int get_next_subfolder_number(const std::string& path) {
    int max_number = 0;
    for (const auto& entry_name : get_directories(path)) {
        try {
            int number = std::stoi(entry_name);
            if (number > max_number) {
                max_number = number;
            }
        } catch (...) {
            continue;
        }
    }
    return max_number + 1;
}

void handle_client(int client_socket) {
    char buffer[1024] = {0};
    std::string current_job_folder;
    std::string current_subfolder;
    std::ofstream current_file;
    std::string current_file_name;

    

    while (true) {
        int valread = read(client_socket, buffer, sizeof(buffer));
        if (valread <= 0) {
            std::cout << "Client disconnected." << std::endl;
            break;
        }

        // Check if the client sent a "CHECK_DONE" command
        std::string command(buffer, valread);
        if (command == "CHECK_DONE") {
            // Check if the job is done
            bool is_done = false;
            std::string done_wav_path;

            {
                std::cout<<current_job_folder<<"da \n";
                std::lock_guard<std::mutex> lock(folder_mutex);
                if (!current_job_folder.empty()) {
                    std::string potential_done_job_folder = current_job_folder;
                    potential_done_job_folder.replace(potential_done_job_folder.find("wip_job_"), 8, "done_job_");
                    done_wav_path = potential_done_job_folder + "/done.wav";
                    std::cout<<done_wav_path<<"\n";
                    if (std::ifstream(done_wav_path)) {
                        is_done = true;
                        current_job_folder = potential_done_job_folder;
                    }
                }
            }

            if (is_done) {
                send_ack(client_socket, "Job ready.");
                send_file(client_socket, done_wav_path);
            } else {
                send_ack(client_socket, "Job not ready.");
            }
            continue;
        }

        // Read the file size
        uint32_t file_size;
        memcpy(&file_size, buffer, sizeof(file_size));
        file_size = ntohl(file_size);

        if (file_size == 0) {
            // End-of-job signal received
            if (!current_job_folder.empty()) {
                std::string final_job_folder = current_job_folder;
                final_job_folder.replace(final_job_folder.find("wip_job_"), 8, "job_");
                std::rename(current_job_folder.c_str(), final_job_folder.c_str());
                //current_job_folder.clear();
                send_ack(client_socket, "Job marked as ready.");
            }
            continue;
        }

        if (file_size > MAX_FILE_SIZE) {
            std::cerr << "File size exceeds the maximum allowed limit." << std::endl;
            send_ack(client_socket, "File too large.");
            break;
        }

        send_ack(client_socket, "Got size.");

        if (is_wav_expected) {
            {
                std::lock_guard<std::mutex> lock(folder_mutex);
                if (current_job_folder.empty()) {
                    current_job_folder = SERVER_FOLDER + "wip_job_" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count());
                    mkdir(current_job_folder.c_str(), 0777);
                }
            }

            current_file_name = current_job_folder + "/sound.wav";
            current_file.open(current_file_name, std::ios::binary);
        } else {
            current_file_name = current_job_folder + "/instructions.txt";
            current_file.open(current_file_name);
        }

        size_t total_read = 0;
        while (total_read < file_size) {
            valread = read(client_socket, buffer, std::min(sizeof(buffer), file_size - total_read));
            if (valread <= 0) {
                std::cerr << "Error reading file data." << std::endl;
                break;
            }
            current_file.write(buffer, valread);
            total_read += valread;
        }
        current_file.close();
        send_ack(client_socket, "Got file.");

        if (is_wav_expected) {
            std::lock_guard<std::mutex> lock(folder_mutex);
            int subfolder_number = get_next_subfolder_number(current_job_folder);
            current_subfolder = current_job_folder + "/" + std::to_string(subfolder_number);
            mkdir(current_subfolder.c_str(), 0777);
            std::string new_wav_path = current_subfolder + "/sound.wav";
            std::rename(current_file_name.c_str(), new_wav_path.c_str());
            is_wav_expected = false;
        } else {
            if (current_file_name.find("instructions.txt") != std::string::npos) {
                std::string new_txt_path = current_subfolder + "/instructions.txt";
                std::rename(current_file_name.c_str(), new_txt_path.c_str());
                is_wav_expected = true;
            }
        }

        memset(buffer, 0, sizeof(buffer));
    }
    close(client_socket);
}

void process_jobs() {
    while (true) {
        std::vector<std::string> job_folders;
        // Get list of job folders
        {
            std::lock_guard<std::mutex> lock(folder_mutex);
            for (const auto &entry_name : get_directories(SERVER_FOLDER)) {
                std::string entry = SERVER_FOLDER + "/" + entry_name;
                if (is_directory(entry) && entry_name.find("job_") == 0) {
                    job_folders.push_back(entry);
                }
            }
        }

        // Process each job
        for (const auto &job_folder : job_folders) {
            std::string command = WORKER_EXEC + " " + job_folder;
            int status = system(command.c_str());
            if (status == 0) {
                // Worker execution successful, mark job as done
                std::string done_job_folder = job_folder;
                done_job_folder.replace(done_job_folder.find("job_"), 4, "done_job_");
                std::rename(job_folder.c_str(), done_job_folder.c_str());

                // Check if done.wav is ready to send
                std::string done_wav_path = done_job_folder + "/sound.wav";
                if (is_directory(done_job_folder) && is_directory(done_wav_path)) {
                    // Notify client that done.wav is ready to be sent
                    // Need to implement this part
                    int client_socket; // Assuming you have access to client socket here
                    send_ack(client_socket, "Job ready."); // Example notification
                    send_file(client_socket, done_wav_path); // Sending done.wav file
                }
            } else {
                std::cerr << "Error processing job in folder: " << job_folder << std::endl;
            }
        }

        // Sleep for some time before checking for new jobs again
        std::this_thread::sleep_for(std::chrono::seconds(10));
    }
}

int main() {
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("Socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(server_fd, MAX_CLIENTS) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    std::thread job_thread(process_jobs);

    std::vector<std::thread> threads;

    while (true) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t *)&addrlen)) < 0) {
            perror("Accept failed");
            continue;
        }
        std::cout << "New connection established." << std::endl;
        threads.emplace_back(handle_client, new_socket);
    }

    for (auto &thread : threads) {
        if (thread.joinable()) {
            thread.join();
        }
    }

    job_thread.join();

    close(server_fd);
    return 0;
}

