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

int HealthTask::execute(){ 
  return 0;
}
int TeapotTask::execute(){ 
  return 0; 
}
int JobListTask::execute(){
  return 0; 
}
int ScriptListTask::execute(){
  return 0; 
}
int DeleteTask::execute(){ 
  return 0; 
}
int JobStatusTask::execute(){
  return 0; 
}
int TerminateTask::execute(){
  return 0; 
}
int StdoutTask::execute(){ 
  return 0;
}
int StderrTask::execute(){
  return 0;
}
int RunTask::execute(){
  return 0; 
}
int UploadTask::execute(){
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

  if(path == "/scripts" || path == "/scripts/upload") {
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