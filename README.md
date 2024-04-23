# Remote File Management System

This repository contains the source code for the File Management System (FMS), which is a system for managing files and directories. It includes both server-side and client-side code.

## Overview

The FMS provides various commands for managing files and directories remotely. These commands include:

- `dirlist -a`: List all files and directories in the current directory.
- `dirlist -t`: List all files and directories in the current directory in tree format.
- `w24fn <filename>`: Retrieve the contents of a file.
- `w24ft <extension list>`: Create a TAR archive containing files with specific extensions.
- `w24fdb <date>`: Create a TAR archive containing files created before or on the specified date.

## Usage

### Server

Compile and run the server code (`server.c`) on the host machine. The server listens for incoming connections from clients and executes commands accordingly.

### Client

Compile and run the client code (`client.c`) on a remote machine. Connect to the server using the specified IP address and port. Use the provided commands to interact with the server.

## Building

To build the server and client executables, use the following commands:

```bash
gcc -o server server.c
gcc -o mirror1 mirror1.c
gcc -o mirror2 mirror2.c
gcc -o client client.c
```

## Requirements
- C compiler (e.g., GCC)
- Linux operating system (for server)
