🎵 C++ Sound Sequencer (Client-Server)

A lightweight C++ sound sequencer that uses WAV files and text-based sequencing instructions to generate music. The application follows a client-server architecture, where the client sends a WAV file and a TXT file defining the sequence. The server processes the instructions and plays the corresponding sound.

✨ Features
📁 Custom Sound Support – Clients can send any WAV file for sequencing.
📜 Text-Based Sequencing – Define playback timing in a simple TXT file.
🌐 Client-Server Model – Remote sound sequencing over a network.
⚡ Efficient C++ Implementation – Low-latency performance.

🔧 How It Works

The client sends a .wav file along with a .txt sequence file to the server.
The server receives the files, processes the sequence, and plays the WAV file accordingly.
The sequence file specifies when and how the WAV file should be played.

🚀 Installation & Usage

Compile the project using a C++ compiler with necessary dependencies (e.g., networking & audio libraries).
Run the server to receive files and play sequences.
Run the client to send a WAV file and sequence instructions.
