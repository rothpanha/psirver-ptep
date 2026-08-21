# Psirver - Concurrent Script Execution Server

Psirver is a C++/POSIX application server that receives HTTP requests, stores Python scripts, and executes them as managed jobs. I developed and extended the server through five milestones in my Operating Systems course.

The project applies operating-system concepts including process creation, multithreading, interprocess communication, signals, synchronization, resource limits, and process cleanup.

## What I Implemented

Across the project milestones, I implemented:

- **Server initialization:** Added command-line port validation, system error logging with `syslog`, PID-file management through `PSIRVER_HOME`, and graceful `SIGINT` shutdown.
- **HTTP request handling:** Parsed GET and POST requests, validated paths and request data, returned appropriate HTTP status codes, and translated valid requests into a hierarchy of executable task objects.
- **Script management:** Implemented endpoints to upload, list, and delete Python scripts. Assigned unique positive script IDs, stored scripts in separate directories, reported file modification times, and applied restrictive filesystem permissions.
- **Concurrent task processing:** Executed server tasks in separate threads and protected shared script and job data using mutexes, lock guards, atomic counters, and smart pointers.
- **Process execution:** Implemented the initial script-running workflow using `fork`, `execvp`, and `wait4`, passing command-line arguments to Python scripts.
- **Job management:** Implemented endpoints to list jobs, report status and exit codes, retrieve captured standard output and error, terminate running jobs, and purge jobs while releasing associated resources.

## How It Works

A client uploads a Python script through an HTTP request. Psirver stores the script in a directory identified by a unique script ID.

When the client requests execution, the server launches Python in a child process and tracks the execution as a job. Pipes capture standard output and standard error while separate threads monitor execution time, output size, and process termination.

Each job reports one of the following states:

- `RUNNING`
- `FINISHED`
- `FAILED`
- `TERMINATED`
- `TIMED_OUT`
- `OUTPUT_LIMITED`

Clients can query job status and output, terminate a running process, or purge a job after it is no longer needed.

## Educational Use and Security

Psirver is an educational operating-systems project, not a secure production server. It executes uploaded scripts with the permissions of the server process and does not provide authentication, HTTPS, containerization, or complete filesystem isolation.

It should only be run locally or in a controlled development environment.