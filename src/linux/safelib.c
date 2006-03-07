/*
Copyright (C) 1997-2006 ZSNES Team ( zsKnight, _Demo_, pagefault, Nach )

http://www.zsnes.com
http://sourceforge.net/projects/zsnes

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either
version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "gblhdr.h"

#include <sys/param.h>
#include <sys/wait.h>
#include <signal.h>
#include <paths.h>
#include <grp.h>

#ifndef OPEN_MAX
#define OPEN_MAX 256
#endif

#include "safelib.h"

//C++ style code in C
#define bool unsigned char
#define true 1
#define false 0


//Introducing secure forking ;) -Nach

//Taken from the secure programming cookbook, somewhat modified
static bool spc_drop_privileges() {
  gid_t newgid = getgid(), oldgid = getegid();
  uid_t newuid = getuid(), olduid = geteuid();

  /* If root privileges are to be dropped, be sure to pare down the ancillary
   * groups for the process before doing anything else because the setgroups()
   * system call requires root privileges.  Drop ancillary groups regardless of
   * whether privileges are being dropped temporarily or permanently.
   */
  if (!olduid) setgroups(1, &newgid);

  if (newgid != oldgid) {
#if !defined(linux)
    setegid(newgid);
    if (setgid(newgid) == -1) return(false);
#else
    if (setregid(newgid, newgid) == -1) return(false);
#endif
  }

  if (newuid != olduid) {
#if !defined(linux)
    seteuid(newuid);
    if (setuid(newuid) == -1) return(false);
#else
    if (setreuid(newuid, newuid) == -1) return(false);
#endif
  }

  /* verify that the changes were successful */

  if (newgid != oldgid && (setegid(oldgid) != -1 || getegid() != newgid))
    return(false);
  if (newuid != olduid && (seteuid(olduid) != -1 || geteuid() != newuid))
    return(false);

  return(true);
}

static int open_devnull(int fd) {
  FILE *f = 0;

  if (!fd) f = freopen(_PATH_DEVNULL, "rb", stdin);
  else if (fd == 1) f = freopen(_PATH_DEVNULL, "wb", stdout);
  else if (fd == 2) f = freopen(_PATH_DEVNULL, "wb", stderr);
  return (f && fileno(f) == fd);
}

static bool array_contains(int *a, size_t size, int key)
{
  size_t i;
  for (i = 0; i < size; i++)
  {
    if (a[i] == key) { return(true); }
  }
  return(false);
}

static bool spc_sanitize_files(int *a, size_t size, int skip)
{
  int fd, fds;
  struct stat st;

  //Make sure all open descriptors other than the standard ones are closed
  if ((fds = getdtablesize()) == -1)
  {
    fds = OPEN_MAX;
  }
  for (fd = 3;  fd < fds;  fd++)
  {
    if ((fd != skip) && !array_contains(a, size, fd)) { close(fd); }
  }

  //Verify that the standard descriptors are open.  If they're not, attempt to
  //open them using /dev/null.  If any are unsuccessful, fail.
  for (fd = 0;  fd < 3;  fd++)
  {
    if (fstat(fd, &st) == -1 && (errno != EBADF || !open_devnull(fd)))
    {
      return(false);
    }
  }
  return(true);
}

//Pass array of file descriptors to leave open
pid_t safe_fork(int *a, size_t size)
{
  int filedes[2];
  if (!pipe(filedes))
  {
    char success = 0;
    pid_t childpid;
    if ((childpid = fork()) == -1) //Fork Failed
    {
      close(filedes[0]);
      close(filedes[1]);
      return(-1);
    }

    if (childpid) //Parent Process
    {
      close(filedes[1]); //Close writing
      read(filedes[0], &success, 1);
      close(filedes[0]);
      if (success)
      {
        return(childpid);
      }
      waitpid(childpid, 0, 0);
      return(-1);
    }


    //This is the child proccess

    close(filedes[0]); //Close reading

    if (!spc_sanitize_files(a, size, filedes[1]) || !spc_drop_privileges())
    {
      write(filedes[1], &success, 1);
      close(filedes[1]);
      _exit(0);
    }

    success = 1;
    write(filedes[1], &success, 1);
    close(filedes[1]);
    return(0);
  }
  return(-1);
}


//Introducing a popen which doesn't return until it knows for sure of program launched or couldn't open -Nach

static char *decode_string(char *str)
{
  size_t str_len = strlen(str), i = 0;
  char *dest = str;

  if ((str_len > 1) && ((*str == '\"') || (*str == '\'')) && (str[str_len-1] == *str))
  {
    memmove(str, str+1, str_len-2);
    str[str_len-2] = 0;
  }

  while (*str)
  {
    if (*str == '\\')
    {
      str++;
    }
    dest[i++] = *str++;
  }
  dest[i] = 0;
  return(dest);
}

static char *find_next_match(char *str, char match_char)
{
  char *pos = 0;

  while (*str)
  {
    if (*str == match_char)
    {
      pos = str;
      break;
    }
    if (*str == '\\')
    {
      if (str[1])
      {
        str++;
      }
      else
      {
        break;
      }
    }
    str++;
  }
  return(pos);
}

static char *get_param(char *str)
{
  static char *pos = 0;
  char *token = 0;

  if (str) //Start a new string?
  {
    pos = str;
  }

  if (pos)
  {
    //Skip delimiters
    while (*pos == ' ') { pos++; }
    if (*pos)
    {
      token = pos;

      //Skip non-delimiters
      while (*pos && (*pos != ' '))
      {
        //Skip quoted characters
        if ((*pos == '\"') || (*pos == '\''))
        {
          char *match_pos = 0;
          if ((match_pos = find_next_match(pos+1, *pos)))
          {
            pos = match_pos;
          }
        }
        //Skip escaped spaces
        if (*pos == '\\') { pos++; }
        pos++;
      }
      if (*pos) { *pos++ = '\0'; }
    }
  }
  return(token);
}

static size_t count_param(char *str)
{
  size_t i = 0;

  while (*str)
  {
    //Skip delimiters
    while (*str == ' ') { str++; }
    //Skip non-delimiters
    while (*str && (*str != ' '))
    {
      //Skip quoted characters
      if ((*str == '\"') || (*str == '\''))
      {
        char *match_str = 0;
        if ((match_str = find_next_match(str+1, *str)))
        {
          str = match_str;
        }
      }
      //Skip escaped spaces
      if (*str == '\\') { str++; }
      str++;
    }
    i++;
  }
  return(i);
}

static char **build_argv(char *str)
{
  size_t argc = count_param(str);
  char **argv = (char **)malloc(sizeof(char *)*(argc+1));

  if (argv)
  {
    char *p, **argp = argv;
    for (p = get_param(str); p; p = get_param(0), argp++)
    {
      *argp = decode_string(p);
    }
    *argp = 0;
    return(argv);
  }
  return(0);
}

static void argv_print(char **argv)
{
  char **argp = argv;
  while (*argp)
  {
    printf("argv[%u]: %s\n", argp-argv, *argp);
    argp++;
  }
  printf("argv[%u]: NULL\n", argp-argv);
}


//Forks, parent is paused until child successfully execs (returns child pid) or child exits (returns failure)
static pid_t parent_pause_fork()
{
  int filedes[2];
  if (!pipe(filedes))
  {
    int pid = fork();
    if (pid == -1) //Failed
    {
      close(filedes[0]);
      close(filedes[1]);
    }
    else if (pid > 0) //Parent
    {
      char success = 1;
      close(filedes[1]);
      read(filedes[0], &success, 1);
      close(filedes[0]);
      if (success)
      {
        return(pid);
      }
      waitpid(pid, 0, 0);
    }
    else //Child
    {
      close(filedes[0]);
      fcntl(filedes[1], F_SETFD, FD_CLOEXEC);
      return(-filedes[1]);
    }
  }
  return(0);
}

static void close_child(pid_t pid)
{
  char success = 0;
  write(-pid, &success, 1);
  close(-pid);
  _exit(0);
}

#define IS_PARENT(x) ((x) > 0)
#define IS_CHILD(x) ((x) < 0)
#define IS_FAIL(x) ((x) == 0)


static struct fp_pid_link
{
  FILE *fp;
  pid_t pid;
  struct fp_pid_link *next;
} fp_pids = { 0, 0, 0 };


FILE *safe_popen(char *command, const char *mode)
{
  //filedes[0] is for reading
  //filedes[1] is for writing.
  int filedes[2];

  if ((*mode == 'r' || *mode == 'w') && !pipe(filedes))
  {
    pid_t childpid = parent_pause_fork();
    if (IS_PARENT(childpid))
    {
      FILE *fp;
      if (*mode == 'r')
      {
        close(filedes[1]);
        fp = fdopen(filedes[0], "r");
      }
      else
      {
        close(filedes[0]);
        fp = fdopen(filedes[1], "w");
      }

      if (fp)
      {
        struct fp_pid_link *link = &fp_pids;
        while (link->next)
        {
          link = link->next;
        }

        link->next = (struct fp_pid_link *)malloc(sizeof(struct fp_pid_link));
        if (link->next)
        {
          link->next->fp = fp;
          link->next->pid = childpid;
          link->next->next = 0;
          return(fp);
        }
        fclose(fp);
      }
      kill(childpid, SIGTERM);
      waitpid(childpid, 0, 0);
    }
    else if (IS_CHILD(childpid))
    {
      char **argv = build_argv(command);
      if (argv)
      {
        if (*mode == 'r')
        {
          dup2(filedes[1], STDOUT_FILENO);
        }
        else
        {
          dup2(filedes[0], STDIN_FILENO);
        }

        if (spc_sanitize_files(0, 0, -childpid) && spc_drop_privileges())
        {
          execvp(argv[0], argv);
        }
        free(argv);
      }
      close_child(childpid);
    }
    close(filedes[0]);
    close(filedes[1]);
  }
  return(0);
}

void safe_pclose(FILE *fp)
{
  struct fp_pid_link *link = &fp_pids;

  while (link->next && link->next->fp != fp)
  {
    link = link->next;
  }
  if (link->next->fp == fp)
  {
    struct fp_pid_link *dellink = link->next;
    fclose(fp);
    waitpid(link->next->pid, 0, 0);
    link->next = link->next->next;
    free(dellink);
  }
}