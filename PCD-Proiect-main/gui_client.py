import tkinter as tk
from tkinter import filedialog, messagebox
import client  # Importing your existing code

class ClientGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("Client GUI")
        
        # Initialize lists to hold file pairs
        self.file_pairs = []

        # Create GUI elements
        self.create_widgets()
    
    def create_widgets(self):
        # Add file pair button
        self.add_pair_button = tk.Button(self.root, text="Add File Pair", command=self.add_file_pair)
        self.add_pair_button.grid(row=0, column=0, padx=10, pady=10)

        # Delete selected file pair button
        self.delete_pair_button = tk.Button(self.root, text="Delete Selected Pair", command=self.delete_file_pair)
        self.delete_pair_button.grid(row=0, column=1, padx=10, pady=10)

        # Listbox to display selected file pairs
        self.file_listbox = tk.Listbox(self.root, selectmode=tk.SINGLE, width=80)
        self.file_listbox.grid(row=1, column=0, columnspan=2, padx=10, pady=10)

        # Start processing button
        self.start_button = tk.Button(self.root, text="Start Processing", command=self.start_processing)
        self.start_button.grid(row=2, column=0, columnspan=2, padx=10, pady=10)

    def add_file_pair(self):
        wav_file = filedialog.askopenfilename(filetypes=[("WAV files", "*.wav")])
        if not wav_file:
            return
        txt_file = filedialog.askopenfilename(filetypes=[("TXT files", "*.txt")])
        if not txt_file:
            return
        self.file_pairs.append((wav_file, txt_file))
        self.update_file_listbox()

    def delete_file_pair(self):
        selected_idx = self.file_listbox.curselection()
        if selected_idx:
            idx = selected_idx[0]
            del self.file_pairs[idx]
            self.update_file_listbox()
        else:
            messagebox.showerror("Error", "Please select a pair to delete.")

    def update_file_listbox(self):
        self.file_listbox.delete(0, tk.END)
        for wav_file, txt_file in self.file_pairs:
            self.file_listbox.insert(tk.END, f"WAV: {wav_file}, TXT: {txt_file}")

    def start_processing(self):
        if not self.file_pairs:
            messagebox.showerror("Error", "Please add at least one pair of files.")
            return

        sock = client.socket.socket(client.socket.AF_INET, client.socket.SOCK_STREAM)
        try:
            sock.connect((client.SERVER_IP, client.PORT))
        except Exception as e:
            messagebox.showerror("Connection Error", f"Connection failed: {e}")
            return
        
        for wav_file, txt_file in self.file_pairs:
            if client.send_file(sock, wav_file) == -1:
                messagebox.showerror("File Error", f"Error sending WAV file: {wav_file}")
                return
            if client.send_file(sock, txt_file) == -1:
                messagebox.showerror("File Error", f"Error sending TXT file: {txt_file}")
                return

        client.send_end_of_job(sock)
        
        while True:
            client.time.sleep(5)
            if client.ask_server_job_status(sock):
                client.receive_done_wav(sock)
                messagebox.showinfo("Success", "Received done.wav file.")
                break
            else:
                print("Server is still processing jobs. Waiting...")

        sock.close()
        messagebox.showinfo("Process Completed", "Processing complete. You can now start a new process.")
        self.file_pairs.clear()
        self.update_file_listbox()

if __name__ == "__main__":
    root = tk.Tk()
    app = ClientGUI(root)
    root.mainloop()

