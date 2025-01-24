// g++-9 -o worker worker.cpp -pthread

#include <iostream>
#include <pthread.h>
#include <unistd.h>
#include <queue>
#include <vector>
#include <string>
#include <sys/wait.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <cstring>

const int MAX_ACTIVE_THREADS = 3; // Maximum number of active threads

pthread_mutex_t mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t cond = PTHREAD_COND_INITIALIZER;
std::queue<std::pair<std::string, std::string>> job_queue; // pair of command and directory
std::vector<pthread_t> active_threads;
std::string initial_working_directory;
bool all_jobs_queued = false; // Indicates whether all jobs have been queued
std::string all_sequenced_files; // Accumulator for sequenced file paths

void fixPaths(std::string& str, const std::string& working_folder) {
    // Replace double slashes with single slashes
    size_t pos = str.find("//");
    while (pos != std::string::npos) {
        str.replace(pos, 2, "/");
        pos = str.find("//", pos + 1);
    }

    // Replace "./" with the working folder
    pos = str.find("./");
    while (pos != std::string::npos) {
        str.replace(pos, 2, working_folder + "/");
        pos = str.find("./", pos + 1);
    }
}


// Function to process a job in a child process
void processJob(const std::pair<std::string, std::string>& job) {
    const std::string& command = job.first;
    const std::string& directory = job.second;

    // Change the working directory
    if (chdir(directory.c_str()) != 0) {
        std::cerr << "chdir failed to " << directory << "\n";
        exit(1);
    }

    std::cout << "Child process ID: " << getpid() << " is processing job: " << command << " in directory: " << directory << "\n";

    // Split the command into program and arguments
    std::vector<std::string> args;
    char* token = std::strtok(const_cast<char*>(command.c_str()), " ");
    while (token != nullptr) {
        std::string arg(token);
        // Replace occurrences of "./" with the initial working directory
        if (arg.find("./") == 0) {
            arg.replace(0, 2, initial_working_directory + "/");
        }
        args.push_back(arg);
        token = std::strtok(nullptr, " ");
    }

    // Prepare arguments for exec
    std::vector<char*> argv;
    for (auto& s : args) {
        argv.push_back(&s[0]);
    }
    argv.push_back(nullptr);

    // Execute the command
    execvp(argv[0], argv.data());

    // If execvp fails
    std::cerr << "execvp failed\n";
    exit(1);
}

void* threadFunction(void* arg) {
    std::string sequenced_files;

    while (true) {
        pthread_mutex_lock(&mutex);
        while (job_queue.empty() && !all_jobs_queued) {
            pthread_cond_wait(&cond, &mutex);
        }

        if (job_queue.empty() && all_jobs_queued) {
            pthread_mutex_unlock(&mutex);
            break;
        }

        auto job = job_queue.front();
        job_queue.pop();
        pthread_mutex_unlock(&mutex);

        pid_t pid = fork();
        if (pid == -1) {
            std::cerr << "Fork failed\n";
        } else if (pid == 0) {
            // Child process
            processJob(job);
        } else {
            // Parent process, wait for the child process to complete
            int status;
            waitpid(pid, &status, 0);

            // If the job was a sequencer command, append its output to sequenced_files
            if (job.first.find("./sequencer") != std::string::npos) {
                std::string sequenced_file = job.second + "/sequenced.wav";
                sequenced_files += " " + sequenced_file;
            }
        }
    }

    // Save sequenced files for this thread
    pthread_mutex_lock(&mutex);
    all_sequenced_files += sequenced_files;
    pthread_mutex_unlock(&mutex);

    return nullptr;
}

void startNewThread() {
    pthread_t thread;
    int rc = pthread_create(&thread, nullptr, threadFunction, nullptr);
    if (rc) {
        std::cerr << "Error: Unable to create thread, " << rc << std::endl;
    } else {
        active_threads.push_back(thread);
    }
}

void readJobsFromFolder(const std::string& folder) {
    DIR* dir = opendir(folder.c_str());
    if (!dir) {
        std::cerr << "Error: Could not open folder " << folder << std::endl;
        return;
    }

    struct dirent* entry;
    while ((entry = readdir(dir)) != nullptr) {
        if (entry->d_type == DT_DIR) {
            std::string subfolder = entry->d_name;
            if (subfolder != "." && subfolder != "..") {
                std::string subfolderPath = folder + "/" + subfolder;
                std::string soundFile = subfolderPath + "/sound.wav";
                std::string instructionsFile = subfolderPath + "/instructions.txt";
                std::string command = "./sequencer " + soundFile + " " + instructionsFile;
                pthread_mutex_lock(&mutex);
                job_queue.emplace(command, subfolderPath);
                pthread_cond_signal(&cond);
                pthread_mutex_unlock(&mutex);
            }
        }
    }

    closedir(dir);
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        std::cerr << "Usage: " << argv[0] << " <folder>" << std::endl;
        return 1;
    }

    // Get the initial working directory
    char cwd[1024];
    if (getcwd(cwd, sizeof(cwd)) != nullptr) {
        initial_working_directory = cwd;
    } else {
        std::cerr << "getcwd() error\n";
        return 1;
    }

    std::string job_folder = argv[1];

    // Read jobs from the given folder
    readJobsFromFolder(job_folder);

    // Signal that all jobs have been queued
    pthread_mutex_lock(&mutex);
    all_jobs_queued = true;
    pthread_cond_broadcast(&cond); // Wake up all threads to check the condition
    pthread_mutex_unlock(&mutex);

    // Start initial threads
    for (int i = 0; i < MAX_ACTIVE_THREADS; ++i) {
        startNewThread();
    }

    // Wait for all threads to complete
    for (pthread_t thread : active_threads) {
        pthread_join(thread, nullptr);
    }
    fixPaths(all_sequenced_files, initial_working_directory);
    std::cout<<all_sequenced_files<<'\n';
    // Mix sequenced sound files
    std::string mixer_command = initial_working_directory + "/mixer ./done.wav" + all_sequenced_files;

    // Change working directory to the job folder
    if (chdir(job_folder.c_str()) != 0) {
        std::cerr << "chdir failed to " << job_folder << "\n";
        return 1;
    }

    // Execute mixer command
    std::cout << "Mixing sequenced sounds...\n";
    system(mixer_command.c_str());
    std::cout << "Mixing completed. Output saved as done.wav\n";


    // Change working directory back to the initial directory
    if (chdir(initial_working_directory.c_str()) != 0) {
        std::cerr << "chdir failed to " << initial_working_directory << "\n";
        return 1;
    }


    std::cout << "Job completed successfully\n";
    return 0;
}
