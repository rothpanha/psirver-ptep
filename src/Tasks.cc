#include "Tasks.hh"

#include <sys/stat.h>
#include <dirent.h>
#include <unistd.h>
#include <cerrno>
#include <cstdlib>
#include <map>
#include <sstream>
#include <iomanip>
#include <fstream>
#include <string>

// get the scripts directory path
static std::string get_scripts_dir() {
    const char* home = getenv("PSIRVER_HOME");
    if (!home) return "";
    return std::string(home) + "/scripts";
}

// accept only directories named with a numeric script ID
static bool all_digits(const char* s) {
    if (!s || !*s) return false;
    for (const char* p = s; *p; ++p) {
        if (*p < '0' || *p > '9') return false;
    }
    return true;
}

// Do the job, reply to the client, and return to the main loop

int HealthTask::execute()
{
  // Return 200 OK and Running as text/plain
  reply(client, "HTTP/1.1 200 OK", "Running");
  return 0;
}
 
int TeapotTask::execute()
{
  // Return 418 I'm a Teapot and Running as text/plain
  reply(client, "HTTP/1.1 418 I'm a Teapot", "Running");
  return 0;
}

int StderrTask::execute()
{
  // --> To be implemented later
  std::cerr << "I will report stderr of job " << job_id << "\n";
  return 0;
}

int DeleteTask::execute()
{
  std::string scripts_dir = get_scripts_dir();
  if (scripts_dir.empty()) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error"); 
      return 0;
  }

  std::string target_dir = scripts_dir + "/" + std::to_string(script_id);
  struct stat st;

  // verify that id refers to an existing uploaded script. Else, return 404 Not Found
  if (stat(target_dir.c_str(), &st) != 0 || !S_ISDIR(st.st_mode)) {
      reply(client, "HTTP/1.1 404 Not Found", "Not Found");
      return 0;
  }

  // find the script file inside the directory
  DIR* dir = opendir(target_dir.c_str());
  if (!dir) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }

  std::string file_to_remove;
  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_name[0] == '.') continue;
      file_to_remove = target_dir + "/" + entry->d_name;
      break;
  }
  closedir(dir);

  // must have a file to delete
  if (file_to_remove.empty()) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error"); 
      return 0;
  }

  chmod(target_dir.c_str(), 0700);
  // remove the script file
  if (unlink(file_to_remove.c_str()) != 0) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error"); 
      return 0;
  }

  // remove the script's subdirectory
  if (rmdir(target_dir.c_str()) != 0) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }

  // return 200 OK and respond with the deleted script id
  std::string id_str = std::to_string(script_id);
  reply(client, "HTTP/1.1 200 OK", id_str.c_str());
  return 0;

}

int RunTask::execute()
{
  // --> To be implemented later
  std::cerr << "I will run script " << script_id << "\n";
  return 0;
}

int JobStatusTask::execute()
{
  // --> To be implemented later
  std::cerr << "I will report status of job " << job_id << "\n";
  return 0;
};

int TerminateTask::execute()
{
  // --> To be implemented later
  std::cerr << "I will terminate job " << job_id << "\n";
  return 0;
}

int StdoutTask::execute()
{
  // --> To be implemented later
  std::cerr << "I will report stdout of job " << job_id << "\n";
  return 0;
} 

int UploadTask::execute()
{
  std::string scripts_dir = get_scripts_dir();
  if (scripts_dir.empty()) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }

  // choose the smallest positive integer not currently in use
  int id = 1;
  struct stat st;
  while (true) {
      std::string check_path = scripts_dir + "/" + std::to_string(id);
      if (stat(check_path.c_str(), &st) != 0) break;
      id++;
  }

  std::string new_dir = scripts_dir + "/" + std::to_string(id);

  // create writable first so we can create the file inside
  if (mkdir(new_dir.c_str(), 0700) != 0) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }

  // create the script file and write content
  std::string file_path = new_dir + "/" + filename;
  std::ofstream out(file_path, std::ios::binary);

  if (!out) {
      rmdir(new_dir.c_str());
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }

  out.write(script.data(), script.size());
  if (!out) {
      out.close();
      unlink(file_path.c_str());
      rmdir(new_dir.c_str());
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }
  out.close();

  // the file should be user-readable and not accessible to group or others
  if (chmod(file_path.c_str(), 0400) != 0) {
      unlink(file_path.c_str());
      rmdir(new_dir.c_str());
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }

  // the new per-script subdirectory should be user-readable and user-executable
  // but not writable or accessible to group or others
  if (chmod(new_dir.c_str(), 0500) != 0) {
      unlink(file_path.c_str());
      rmdir(new_dir.c_str());
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error");
      return 0;
  }

  // return 200 OK and include the new assigned script id
  std::string id_str = std::to_string(id);
  reply(client, "HTTP/1.1 200 OK", id_str.c_str());
  return 0;
}

int ScriptListTask::execute()
{
  std::string scripts_dir = get_scripts_dir();
  if (scripts_dir.empty()) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error"); 
      return 0;
  }

  DIR* dir = opendir(scripts_dir.c_str());
  if (!dir) {
      reply(client, "HTTP/1.1 500 Internal Server Error", "Internal Server Error"); 
      return 0;
  }

  struct ScriptInfo {
      std::string filename;
      time_t mtime;
  };

  // sorted by numeric id
  std::map<int, ScriptInfo> scripts;

  struct dirent* entry;
  while ((entry = readdir(dir)) != nullptr) {
      if (entry->d_name[0] == '.') continue;

      // require directory name to be all digits
      if (!all_digits(entry->d_name)) continue;

      int id = std::stoi(entry->d_name);
      if (id <= 0) continue;

      std::string sub_dir_path = scripts_dir + "/" + entry->d_name;

      struct stat dst;
      if (stat(sub_dir_path.c_str(), &dst) != 0 || !S_ISDIR(dst.st_mode)) {
          continue;
      }

      DIR* subdir = opendir(sub_dir_path.c_str());
      if (!subdir) continue;

      struct dirent* subentry;
      while ((subentry = readdir(subdir)) != nullptr) {
          if (subentry->d_name[0] == '.') continue;

          std::string file_path = sub_dir_path + "/" + subentry->d_name;
          struct stat fst;
          if (stat(file_path.c_str(), &fst) == 0 && S_ISREG(fst.st_mode)) {
              scripts[id] = { subentry->d_name, fst.st_mtime };
          }
          break;
      }
      closedir(subdir);
  }
  closedir(dir);

  // if no scripts, body must be empty
  if (scripts.empty()) {
      reply(client, "HTTP/1.1 200 OK", "");
      return 0;
  }

  std::stringstream ss;
  bool first = true;

  for (const auto& pair : scripts) {
      int id = pair.first;
      const ScriptInfo& info = pair.second;

      struct tm* tm_info = localtime(&info.mtime);
      if (!tm_info) continue;

      if (!first) ss << "\n";
      first = false;

      ss << id << "," << info.filename << ","
          << std::put_time(tm_info, "%m/%d/%Y %H:%M:%S");
  }

  // return the current script listing as text/plain
  std::string result = ss.str();
  reply(client, "HTTP/1.1 200 OK", result.c_str());
  return 0;
}

int JobListTask::execute()
{
  // --> To be implemented later
  std::cerr << "I will report list of jobs\n";
  return 0;
}
