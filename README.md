HTTP Server With Multi-Threading Patching and Logging

By Nicholas Henderson

Basic Info

This program runs a basic HTTP server which supports multi-threading, patching and logging. This was an assignment made for CSE 130 in November, 2019. Please look inside for a sample of my coding practices. The makefile was provided by instructors, but the rest of the files are my own work.

Files:
httpserver.cpp: Contains the code for the HTTP server specified in the assignment.

Compile with "make".

Run with ./httpserver (address or hostname) [port number] [-N (number of threads)] [-a (map file)] [-l (logging file)]

Address or hostname is mandatory. Port number is not. Note that if you run with the default port number, or any other small port number, you will be unable to make requests without using "sudo" before the command to run the program.

-N (number) sets the server to run with (number) threads to handle requests.

-a (file) sees if the mapping file already exists. If it does then it trasnfers values on the mapping file to the local on disk hash table. If the mapping file does not exist already it just creates a new one and proceeds to execute.

-l (file) sets the server to enable logging and logs to (file)

Makefile: Compiles httpserver.cpp with the compiler flags specified in the assignment instructions.

Run with "make".

Also supports "make clean" and "make spotless".

README.md: This file, which includes information about the files submitted, as well as the assumptions made about the assignment.
