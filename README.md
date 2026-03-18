# Technical-Support-Service
# Description
This C program implements a console-based support service system that manages customer requests across multiple subdivisions. Each request has an ID, username, priority, timestamp, and optional dependencies on other requests. Requests are stored in priority queues within subdivisions. The system features dynamic queue balancing (splitting/merging), a stack for canceled requests, dependency tracking, search by ID or username, and serialization to a text file. A user-friendly menu allows interactive management of requests.

# Features
Multiple subdivisions – three independent subdivisions, each containing one or more priority queues.

Priority‑based queuing – requests are ordered by priority (1–5, lower number = higher priority).

Dynamic queue management – queues automatically split when exceeding MAX_QUEUE_SIZE (5) and merge when the total size in a subdivision falls below MIN_TOTAL_SIZE (3).

Dependencies – a request can depend on other requests; it cannot be processed until all its dependencies (still in queues) are resolved.

Request processing – process the highest‑priority request in a subdivision, with options to handle dependencies manually or recursively.

Cancel / restore – canceled requests are moved to a stack and can be restored later.

Search – find a request by its ID or by username (searches both queues and the cancel stack).

Serialization – save the entire system state to support_data.txt and load it back.

Memory safety – dynamic memory allocation for dependencies and queues; all memory is freed on exit.

Cross‑platform – uses strtok_r / strtok_s for thread‑safe tokenization; Windows console supports Cyrillic (CP‑1251).

# Requirements
C compiler with C99 support (e.g., GCC, MSVC).

Standard C library.

Windows (for proper console Cyrillic output) – but the core logic is platform‑independent.

# Installation
Download or clone the source file sup_serv.c.

Compile using a C compiler. Example with GCC:

bash
gcc -o sup_serv sup_serv.c -std=c99
(Optional) Create an empty support_data.txt file in the same directory if you want to start with saved data.

Run the executable:

bash
./sup_serv

# License
This project is provided for educational purposes. No explicit license is specified. The code may be used and modified at your own risk.
