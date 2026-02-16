- Line 33: `static char g_pid_path[4096] = {0};`
  * The number looks arbitrary. Use constant PATH_MAX.
  
- Line 39: `char buf[512];`
  * Same as above. Use std::string and let C++ worry about sizes.
  
- Line 60: `_exit(EXIT_SUCCESS);`
  * Just ise `exit`, what's wrong with it?

- Line 122: `log_errno(LOG_ERR, "fcntl(FD_CLOEXEC) failed");`
  * Not an error but a warning/notice.
  
- Lines 276, 288, and 322: `65535`,`0644`
  * Do not use "magic" (unexplained) numbers.
  
- Line 302: `std::cerr << "error: PSIRVER_HOME is not set\n";`
  * According to your logic, it's "PSIRVER_HOME is not set OR EMPTY". Be accurate. 
  
- Line 314: Use accurate error reporting, like on Line 310.

- Line 330: `(void)fchmod(pidfd, 0644);`
  * Never ignore a value returned by an I/O function. Check it and report an error, if needed. 
  
- Line 333: `char pidbuf[64];`
  * Too much, and unnecessary (use C++ strings).
