#pragma once
#include <iostream>
#include <cstring>
#include <vector>
#include <string>
#include <algorithm>
#include <unistd.h>


void reply(int client, const char *status_line, const char *body);

class Task {
protected:
  // `int client` is the socket for sending the status back to the
  // client
  int client;
  
public:
  Task(int client) : client(client) {};
  
  virtual ~Task() {
  if (client >= 0) {
    fsync(client);
    close(client);
  }
}
  virtual int execute() = 0;	// Execute the task
  
  static Task *construct(int client, std::string headers); // GET
  static Task *construct(int client, std::string headers, std::string body); // POST
}; 

class HealthTask : public Task { // GET /health
public:
  HealthTask(int client) : Task(client) {
     std::cerr << "HealthTask\n";
  };
  int execute();
};

class TeapotTask : public Task { // GET /teapot
public:
  TeapotTask(int client) : Task(client) {
    std::cerr << "TeapotTask\n";
  };
  int execute();
};

class JobListTask : public Task { // GET /jobs
public:
  JobListTask(int client) : Task(client) {
    std::cerr << "JobListTask\n";
  };
  int execute();
};

class ScriptListTask : public Task { // GET /scripts
public:
  ScriptListTask(int client) : Task(client) {
    std::cerr << "ScriptListTask\n";
  };
  int execute();
};

class DeleteTask : public Task { // GET /scripts/<id>/delete
private:
  int script_id;
public:
  DeleteTask(int client, int id) : Task(client), script_id(id) {
    std::cerr << "DeleteTask script_id=" << id << "\n";
  };
  int execute();
};

class JobStatusTask : public Task { // GET /jobs/<id>
private:
  int job_id;
public:
  JobStatusTask(int client, int id) : Task(client), job_id(id) {
    std::cerr << "JobStatusTask job_id=" << id << "\n";
  };
  int execute();
}; 

class TerminateTask : public Task { // GET /jobs/<id>/terminate
private:
  int job_id;
public:
  TerminateTask(int client, int id) : Task(client), job_id(id) {
    std::cerr << "TerminateTask job_id=" << id << "\n";
  };
  int execute();
};

class StdoutTask : public Task { // GET /jobs/<id>/stdout
private:
  int job_id;
public:
  StdoutTask(int client, int id) : Task(client), job_id(id) {
     std::cerr << "StdoutTask job_id=" << id << "\n";
  };
  int execute();
}; 

class StderrTask : public Task { // GET /jobs/<id>/stderr
private:
  int job_id;
public:
  StderrTask(int client, int id) : Task(client), job_id(id) {
    std::cerr << "StderrTask job_id=" << id << "\n";
  };
  int execute();
}; 

class RunTask : public Task { // POST /scripts/<id>/run + args
private:
  int script_id;
  std::vector<std::string> args;
public:
  RunTask(int client, int id, std::vector<std::string> a)
    : Task(client), script_id(id), args(std::move(a)) {
    std::cerr << "RunTask script_id=" << id
              << " argc=" << args.size() << "\n";
    };
  int execute();
};

class UploadTask : public Task { // POST /scripts/upload
private:
  std::string filename;
  std::string script;
public:
  UploadTask(int client, std::string fn, std::string s)
    : Task(client), filename(std::move(fn)), script(std::move(s)) {
    std::cerr << "UploadTask filename=\"" << filename
              << "\" bytes=" << script.size() << "\n";
    };
  int execute();
};