Here's a comprehensive `README.md` file for your project:

---

# Distributed File System Project

This project implements a distributed file system using C and socket programming. It consists of three servers (Smain, Spdf, Stext) and multiple client connections. The clients interact only with the Smain server, which manages the distribution of files across the other servers based on file types.

## Project Structure

```
Distributed_File_System_Project/
│
├── SmainServer/
│   ├── smain.c          # Main server handling client requests and dispatching files
│   ├── smain.h          # Header file (if needed)
│   └── Makefile         # Makefile to compile Smain server
│
├── SpdfServer/
│   ├── spdf.c           # Server handling .pdf files
│   ├── spdf.h           # Header file (if needed)
│   └── Makefile         # Makefile to compile Spdf server
│
├── StextServer/
│   ├── stext.c          # Server handling .txt files
│   ├── stext.h          # Header file (if needed)
│   └── Makefile         # Makefile to compile Stext server
│
├── Client/
│   ├── client.c         # Client code to interact with Smain server
│   └── Makefile         # Makefile to compile Client code
│
├── README.md            # This README file
└── Makefile             # Root Makefile to compile all components
```

## Features

- **File Upload (`ufile`)**: Upload files to the Smain server, which forwards them to the appropriate server based on the file extension.
  - `.c` files are stored locally on the Smain server.
  - `.pdf` files are forwarded to the Spdf server.
  - `.txt` files are forwarded to the Stext server.

- **File Download (`dfile`)**: Download files from the Smain server. Smain fetches files from the respective servers if needed.

- **File Deletion (`rmfile`)**: Delete files from the distributed file system. Smain handles the deletion request and forwards it to the appropriate server.

- **Directory Listing (`display`)**: List files in a specified directory on the distributed file system. Smain requests the file list from the relevant server.

- **Tarball Creation (`dtar`)**: Create a tarball of all files with a specific extension (e.g., `.pdf`, `.txt`, `.c`) on the respective servers.

## Installation

### Prerequisites

- GCC (for compiling C code)
- Make (for managing the build process)
- Linux or Unix-based system

### Compiling the Project

1. **Clone the repository**:
   ```bash
   git clone https://github.com/yourusername/Distributed_File_System_Project.git
   cd Distributed_File_System_Project
   ```

2. **Compile all components**:
   ```bash
   make
   ```

   This will compile the Smain, Spdf, Stext servers, and the client code.

3. **Compile individual components**:
   Navigate to the respective directories and run `make` to compile specific components:
   ```bash
   cd SmainServer
   make

   cd ../SpdfServer
   make

   cd ../StextServer
   make

   cd ../Client
   make
   ```

## Usage

### Running the Servers

1. **Start Smain server**:
   ```bash
   cd SmainServer
   ./smain
   ```

2. **Start Spdf server**:
   ```bash
   cd SpdfServer
   ./spdf
   ```

3. **Start Stext server**:
   ```bash
   cd StextServer
   ./stext
   ```

### Running the Client

1. **Start the client**:
   ```bash
   cd Client
   ./client
   ```

2. **Client Commands**:
   - **Upload a file**:
     ```bash
     ufile /path/to/local/file.txt ~smain/remote/directory
     ```
   - **Download a file**:
     ```bash
     dfile ~smain/remote/directory/file.txt
     ```
   - **Delete a file**:
     ```bash
     rmfile ~smain/remote/directory/file.txt
     ```
   - **List files in a directory**:
     ```bash
     display ~smain/remote/directory
     ```
   - **Create a tarball of files**:
     ```bash
     dtar .txt
     ```

### Example Workflow

1. **Uploading a File**:
   - Upload a `.txt` file to the `Smain` server:
     ```bash
     ufile /home/user/document.txt ~smain/docs/
     ```

2. **Downloading a File**:
   - Download the `.txt` file from the `Smain` server:
     ```bash
     dfile ~smain/docs/document.txt
     ```

3. **Deleting a File**:
   - Delete the `.txt` file from the `Smain` server:
     ```bash
     rmfile ~smain/docs/document.txt
     ```

4. **Listing Files in a Directory**:
   - List all files in the `docs` directory:
     ```bash
     display ~smain/docs/
     ```

5. **Creating a Tarball**:
   - Create a tarball of all `.txt` files:
     ```bash
     dtar .txt
     ```

## Contributing

1. **Fork the repository**.
2. **Create a new branch**:
   ```bash
   git checkout -b feature-branch
   ```
3. **Commit your changes**:
   ```bash
   git commit -m "Add new feature"
   ```
4. **Push to the branch**:
   ```bash
   git push origin feature-branch
   ```
5. **Create a pull request**.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.

## Contact

For any questions or suggestions, feel free to open an issue or contact me at [your-email@example.com].

---

### Notes:

- Replace the placeholders (e.g., `yourusername`, `your-email@example.com`) with your actual information.
- Adjust the paths and file names if they differ from your actual project structure.
