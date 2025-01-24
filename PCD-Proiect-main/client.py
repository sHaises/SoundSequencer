import socket
import os
import struct
import time

PORT = 8080
SERVER_IP = "127.0.0.1"

def get_ack(sock):
    ack_buffer = sock.recv(1024).decode()
    print(f"Server: {ack_buffer}")

def send_file(sock, file_path):
    try:
        file_size = os.path.getsize(file_path)
        with open(file_path, 'rb') as file:
            # Send file size first
            size_to_send = struct.pack('!I', file_size)
            sock.send(size_to_send)
            get_ack(sock)

            # Send file contents in chunks
            while True:
                chunk = file.read(1024)
                if not chunk:
                    break
                sock.sendall(chunk)

            get_ack(sock)
            return 0
    except FileNotFoundError:
        print(f"Error opening file: {file_path}")
        return -1

def send_end_of_job(sock):
    end_signal = struct.pack('!I', 0)
    sock.send(end_signal)
    get_ack(sock)

def ask_server_job_status(sock):
    status_request = "CHECK_DONE"
    sock.sendall(status_request.encode())

    response = sock.recv(1024).decode()
    print(f"Server response to CHECK_DONE: {response}")

    return "Job ready." in response

def send_ack(sock):
    ack = struct.pack('!I', 0)
    sock.send(ack)
    print(f"Sending ack: {0}")

def receive_done_wav(sock):
    send_ack(sock)
    print("Receiving done.wav size...")
    file_size = struct.unpack('!I', sock.recv(4))[0]
    print(f"Received file size: {file_size}")

    send_ack(sock)

    with open('done.wav', 'wb') as file:
        total_received = 0
        while total_received < file_size:
            chunk = sock.recv(1024)
            if not chunk:
                break
            file.write(chunk)
            total_received += len(chunk)

    print("Received done.wav file.")
    send_ack(sock)

def main():
    ask = 0
    while ask == 0:
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)

        try:
            sock.connect((SERVER_IP, PORT))
        except Exception as e:
            print(f"Connection failed: {e}")
            return -1

        while True:
            while True:
                try:
                    wav_path = input("Input wav: ")
                    if send_file(sock, wav_path) == 0:
                        break
                except Exception as e:
                    print(f"Error sending file: {e}")

            while True:
                try:
                    txt_path = input("Input text: ")
                    if send_file(sock, txt_path) == 0:
                        break
                except Exception as e:
                    print(f"Error sending file: {e}")

            add_another = input("Add another sound? <y/n>: ")
            if add_another.lower() != "y":
                send_end_of_job(sock)
                break

        while True:
            time.sleep(5)
            if ask_server_job_status(sock):
                receive_done_wav(sock)
                break
            else:
                print("Server is still processing jobs. Waiting...")

        sock.close()

        nex = input("Process another song? <y/n>: ")
        if nex.lower() != "y":
            ask = 1

    return 0

if __name__ == "__main__":
    main()

