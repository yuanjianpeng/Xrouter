#ifndef LIBBASE_FILESYSTEM_H
#define LIBBASE_FILESYSTEM_H

/* return number of data readed or error */
int safe_read(int fd, int pos, char *buf, int len);

/* return 0 if all data written, or error */
int safe_write(int fd, int pos, const char *buf, int len);

/* return 0 if ok, or error */
int set_cloexec_flag(int fd);


/* read a line into buf (or partial if the line is too long)
   a terminated \0 is always written to buf,
   return the string length of buf after read 
   if open or read failed, a negative error number is returned.
 */
int read_txt_file(const char *path, char *buf, int bufsize);

/* open path (create if not exists), then lock it,
   return fd if lock ok. 
   if open, or flock failed, a nagative error number is returned
 */
int open_lock_file(const char *path);

/* write pid to fd */
int write_pid_file(int fd, int pid);

/* dup /dev/null to stdout & stderr */
void daemon_std_fd();

/* make directories of all components */
int mkdir_p(char *path, unsigned mode);

/* make directories of components before last SLASH */
int mkdir_l(char *path, unsigned mode);

#endif

