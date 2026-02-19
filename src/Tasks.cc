#include "Tasks.hh"
#include <algorithm>
#include <sstream>
#include <iostream>
#include <vector>
#include <string>

// HTTP protocol and URL parsing constants
const std::string PREFIX_SCRIPTS = "/scripts/";
const std::string PREFIX_JOBS = "/jobs/";
const std::string PREFIX_ARGS = "args=";
const std::string HEADER_BOUND = "boundary=";
const std::string HEADER_FNAME = "filename=\"";
const std::string CRLF_2 = "\r\n\r\n";

// The Kernel State
struct Script { int id; std::string name, text; };
struct Job { int id, script_id; std::string script_name, status, out, err; };

static std::vector<Script> g_scripts;
static std::vector<Job> g_jobs;
static int g_next_sid = 1;
static int g_next_jid = 1;

// Ensures the Task destructor does not try to close an already-closed socket.
static void send_and_detach(int &client, const char *status, const std::string &body){
  reply(client, status, body.c_str());
  client = -1; 
}
// Checks if a string s starts with the given prefix.
static bool starts_with(const std::string& s, const std::string& prefix){ 
    return s.rfind(prefix, 0) == 0; 
}
// Extracts the request path from the first line of the HTTP headers.
static std::string get_path(const std::string& headers){
  size_t end_line = headers.find('\n');
  std::string line = headers.substr(0, end_line); 
  size_t first_space = line.find(' ');
  if(first_space == std::string::npos) return "";
  size_t second_space = line.find(' ', first_space + 1);
  if(second_space == std::string::npos) return "";
  return line.substr(first_space + 1, second_space - first_space - 1);
}
// Parses a numeric ID from the path string
// Updates end_pos to point to the character after the number.
// Returns -1 if no number is found.
static int parse_id(const std::string& path, size_t offset, size_t& end_pos){
  end_pos = offset;
  while(end_pos < path.size() && isdigit(path[end_pos])) end_pos++;
  if(end_pos == offset) return -1;
  return std::stoi(path.substr(offset, end_pos - offset));
}
// Checks if a specific header or substring exists in the headers.
static bool header_has(const std::string& h, const char* needle){ 
    auto it = std::search(h.begin(), h.end(), needle, needle + strlen(needle),
        [](char a, char b) { return std::tolower(a) == std::tolower(b); });
    return it != h.end();
}
// Extracts the boundary parameter from the content-type header.
static std::string get_boundary(const std::string& h) {
    size_t pos = h.find(HEADER_BOUND);
    if(pos == std::string::npos) return "";
    size_t end = h.find("\r\n", pos);
    return "--" + h.substr(pos + HEADER_BOUND.length(), end - (pos + HEADER_BOUND.length()));
}

// Decodes URL-encoded characters.
static std::string url_decode(const std::string& s){
  std::string ret;
  for(size_t i=0; i<s.size(); i++){
    if(s[i] == '+'){ ret += ' '; }
    else if(s[i] == '%' && i + 2 < s.size()){
      int code;
      if(sscanf(s.substr(i+1, 2).c_str(), "%x", &code) == 1){
          ret += static_cast<char>(code);
          i += 2;
      } else ret += s[i];
    } else { ret += s[i]; }
  }
  return ret;
}

// Helper to find a script in the global vector by its id.
static Script* find_script(int id){ for(auto& s : g_scripts) if(s.id == id) return &s; return nullptr; }

// Helper to find a job in the global vector by its id.
static Job* find_job(int id){ for(auto& j : g_jobs) if(j.id == id) return &j; return nullptr; }

// GET /health: Returns 200 OK to indicate server is running.
int HealthTask::execute() {
  send_and_detach(client, "HTTP/1.1 200 OK", "OK\n");
  return 0;
}
// GET /teapot: Returns 418 I'm a teapot.
int TeapotTask::execute() {
  send_and_detach(client, "HTTP/1.1 418 I'm a teapot", "I'm a teapot\n");
  return 0;
}

// GET /jobs: Lists all currently running jobs.
int JobListTask::execute() {
  std::string body;
  for(const auto& j : g_jobs) {
      if(j.status == "running") {
          body += std::to_string(j.id) + "," + std::to_string(j.script_id) + "," + j.script_name + "\n";
      }
  }
  send_and_detach(client, "HTTP/1.1 200 OK", body);
  return 0;
}

// GET /scripts: Lists all uploaded scripts.
int ScriptListTask::execute() {
  std::string body;
  for(const auto& s : g_scripts) {
      body += std::to_string(s.id) + "," + s.name + "\n";
  }
  send_and_detach(client, "HTTP/1.1 200 OK", body);
  return 0;
}

// GET /scripts/<id>/delete: Removes a script from memory if it exists.
int DeleteTask::execute() {
  if(!find_script(script_id)) {
      send_and_detach(client, "HTTP/1.1 404 Not Found", "Not Found");
      return 0;
  }
  g_scripts.erase(std::remove_if(g_scripts.begin(), g_scripts.end(),
      [&](const Script& s){ return s.id == script_id; }), g_scripts.end());
  send_and_detach(client, "HTTP/1.1 200 OK", "OK\n");
  return 0;
}

// GET /jobs/<id>: Returns the status of a specific job.
int JobStatusTask::execute() {
  Job* j = find_job(job_id);
  if(!j) { send_and_detach(client, "HTTP/1.1 404 Not Found", "Not Found"); return 0; }
  std::string body = std::to_string(j->script_id) + "," + j->script_name + "," + j->status + "\n";
  send_and_detach(client, "HTTP/1.1 200 OK", body);
  return 0;
}

// GET /jobs/<id>/terminate: Stops a job and removes it from the list.
int TerminateTask::execute() {
  if(!find_job(job_id)) { send_and_detach(client, "HTTP/1.1 404 Not Found", "Not Found"); return 0; }
  g_jobs.erase(std::remove_if(g_jobs.begin(), g_jobs.end(),
      [&](const Job& j){ return j.id == job_id; }), g_jobs.end());
  send_and_detach(client, "HTTP/1.1 200 OK", "OK\n");
  return 0;
}

// GET /jobs/<id>/stdout: Returns standard output. Returns 202 if still running.
int StdoutTask::execute() {
  Job* j = find_job(job_id);
  if(!j) { send_and_detach(client, "HTTP/1.1 404 Not Found", "Not Found"); return 0; }
  if(j->status == "running") send_and_detach(client, "HTTP/1.1 202 Accepted", "");
  else send_and_detach(client, "HTTP/1.1 200 OK", j->out);
  return 0;
} 

// GET /jobs/<id>/stderr: Returns standard error. Returns 202 if still running.
int StderrTask::execute() {
  Job* j = find_job(job_id);
  if(!j) { send_and_detach(client, "HTTP/1.1 404 Not Found", "Not Found"); return 0; }
  if(j->status == "running") send_and_detach(client, "HTTP/1.1 202 Accepted", "");
  else send_and_detach(client, "HTTP/1.1 200 OK", j->err);
  return 0;
}

// POST /scripts/<id>/run: Creates a new job for the given script.
// Returns 303 See Other redirecting to the new job's status page.
int RunTask::execute() {
  Script* s = find_script(script_id);
  if(!s) { send_and_detach(client, "HTTP/1.1 404 Not Found", "Not Found"); return 0; }
  Job j; j.id = g_next_jid++; j.script_id = s->id; j.script_name = s->name; j.status = "running";
  g_jobs.push_back(j);
  
  std::string loc = PREFIX_JOBS + std::to_string(j.id);
  std::string body = std::to_string(j.id) + "\n";
  std::string resp = "HTTP/1.1 303 See Other\r\nLocation: " + loc + 
                     "\r\nContent-Length: " + std::to_string(body.size()) + "\r\nConnection: close\r\n\r\n";
  
  if (write(client, resp.c_str(), resp.size()) < 0 || write(client, body.c_str(), body.size()) < 0) {
      std::cerr << "Write failed" << std::endl;
  }
  close(client);
  client = -1;
  return 0;
}

// POST /scripts/upload: Saves a new script to memory and returns its id.
int UploadTask::execute() {
  int id = g_next_sid++;
  g_scripts.push_back({id, filename, script});
  send_and_detach(client, "HTTP/1.1 200 OK", std::to_string(id) + "\n");
  return 0;
}

// This function parses the headers and returns one of the GET task
// objects
Task *Task::construct(int client, std::string headers)
{
  std::string path = get_path(headers);
  std::cerr << "GET " << path << std::endl; 
  if(path.empty()) { reply(client, "HTTP/1.1 400 Bad Request", "Bad Request"); return nullptr; }

  if(path == "/health") return new HealthTask(client);
  if(path == "/teapot") return new TeapotTask(client); 
  if(path == "/jobs") return new JobListTask(client);
  if(path == "/scripts") return new ScriptListTask(client);

  if(starts_with(path, PREFIX_SCRIPTS)) {
      size_t end_pos = 0;
      int id = parse_id(path, PREFIX_SCRIPTS.length(), end_pos); // Using constant length
      if(id >= 0 && path.substr(end_pos) == "/delete") return new DeleteTask(client, id);
      reply(client, "HTTP/1.1 404 Not Found", "Script Not Found");
      return nullptr;
  }

  if(starts_with(path, PREFIX_JOBS)) {
      size_t end_pos = 0;
      int id = parse_id(path, PREFIX_JOBS.length(), end_pos); // Using constant length
      if(id < 0) { reply(client, "HTTP/1.1 404 Not Found", "Invalid Job ID"); return nullptr; }
      std::string suffix = path.substr(end_pos);
      if(suffix.empty()) return new JobStatusTask(client, id);
      if(suffix == "/terminate") return new TerminateTask(client, id);
      if(suffix == "/stdout") return new StdoutTask(client, id);
      if(suffix == "/stderr") return new StderrTask(client, id);
  }
  reply(client, "HTTP/1.1 404 Not Found", "Not Found");
  return nullptr;
}

// This function parses the headers and the body returns one of the
// POST task objects
Task *Task::construct(int client, std::string headers, std::string body)
{
  std::string path = get_path(headers);
  std::cerr << "POST " << path << std::endl;
  if(path.empty()) { reply(client, "HTTP/1.1 400 Bad Request", "Bad Request"); return nullptr; }

  if(starts_with(path, PREFIX_SCRIPTS)) {
      size_t end_pos = 0;
      int id = parse_id(path, PREFIX_SCRIPTS.length(), end_pos);
      if(id >= 0 && path.substr(end_pos) == "/run") {
          std::vector<std::string> args;
          size_t p = body.find(PREFIX_ARGS);
          if(p != std::string::npos) {
              std::string raw = url_decode(body.substr(p + PREFIX_ARGS.length())); 
              std::stringstream ss(raw);
              std::string segment;
              while(std::getline(ss, segment, ',')) if(!segment.empty()) args.push_back(segment);
          }
          return new RunTask(client, id, args);
      }
  }

  if(path == "/scripts/upload") {
      if(!header_has(headers, "multipart/form-data")) {
          reply(client, "HTTP/1.1 415 Unsupported Media Type", "Unsupported Media Type");
          return nullptr;
      }
      std::string boundary = get_boundary(headers);
      if(boundary.empty()) { reply(client, "HTTP/1.1 400 Bad Request", "Missing Boundary"); return nullptr; }

      size_t part_start = body.find(boundary);
      size_t header_end = body.find(CRLF_2, part_start);
      if(part_start == std::string::npos || header_end == std::string::npos) {
           reply(client, "HTTP/1.1 400 Bad Request", "Bad Multipart Format"); return nullptr;
      }

      std::string part_headers = body.substr(part_start, header_end - part_start);
      size_t fn_pos = part_headers.find(HEADER_FNAME);
      if(fn_pos == std::string::npos) { reply(client, "HTTP/1.1 400 Bad Request", "Filename missing"); return nullptr; }
      fn_pos += HEADER_FNAME.length(); 
      std::string filename = part_headers.substr(fn_pos, part_headers.find("\"", fn_pos) - fn_pos);

      size_t content_start = header_end + CRLF_2.length(); 
      size_t content_end = body.find(boundary, content_start);
      if(content_end == std::string::npos) { reply(client, "HTTP/1.1 400 Bad Request", "End boundary missing"); return nullptr; }

      if(content_end > 2 && body[content_end-2] == '\r' && body[content_end-1] == '\n') content_end -= 2;
      return new UploadTask(client, filename, body.substr(content_start, content_end - content_start));
  }
  reply(client, "HTTP/1.1 404 Not Found", "Not Found");
  return nullptr;
}