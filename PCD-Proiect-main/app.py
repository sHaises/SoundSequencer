from flask import Flask, request, redirect, url_for, send_file, render_template
import os
import socket
import time
import client  # Make sure client.py is in the same directory as app.py

app = Flask(__name__)
app.config['UPLOAD_FOLDER'] = 'uploads'
os.makedirs(app.config['UPLOAD_FOLDER'], exist_ok=True)

@app.route('/')
def index():
    return render_template('index.html')

@app.route('/upload', methods=['POST'])
def upload_file():
    try:
        wav_paths = []
        txt_paths = []

        # Iterate through uploaded files
        for key in request.files:
            if key.startswith('wav_file'):
                wav_file = request.files[key]
                txt_key = key.replace('wav_file', 'txt_file')
                txt_file = request.files.get(txt_key)

                if txt_file:
                    # Save files to upload folder
                    wav_path = os.path.join(app.config['UPLOAD_FOLDER'], wav_file.filename)
                    txt_path = os.path.join(app.config['UPLOAD_FOLDER'], txt_file.filename)
                    wav_file.save(wav_path)
                    txt_file.save(txt_path)
                    wav_paths.append(wav_path)
                    txt_paths.append(txt_path)

        # Create a socket and connect to the server
        sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        try:
            sock.connect((client.SERVER_IP, client.PORT))
        except Exception as e:
            return f"Connection failed: {e}"

        # Send each pair of files
        for wav_path, txt_path in zip(wav_paths, txt_paths):
            if client.send_file(sock, wav_path) == -1:
                return f"Error sending WAV file: {wav_path}"
            if client.send_file(sock, txt_path) == -1:
                return f"Error sending TXT file: {txt_path}"

        client.send_end_of_job(sock)

        # Wait for server to process
        while True:
            time.sleep(5)
            if client.ask_server_job_status(sock):
                client.receive_done_wav(sock)
                break

        sock.close()
        return send_file('done.wav', as_attachment=True)

    except Exception as e:
        return f"Error processing files: {e}"

if __name__ == '__main__':
    app.run(debug=True)

