/*
* The use of this code is not allow
* The task was to write a program that works
* As a simple command interpreter (shell) for UNIX.
* The idea is that the program should allow the user to enter
* Commands until he / she chooses to exit the command prompt with the command "exit".
*
*Copyright Created by Luthon Hagvinprice on 06/12/14.
* kone@kth.se
*
* Student of ICT KTH Kista information than communication technology
*/
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>


/* the maximum number of processes we spawn is 4
   it could be [printenv, sort, less] or
   [printenv, grep, sort, less]
*/
#define MAX_PROCS 4
/* maximum number of arguments which can be passed to digenv.
   these are the arguments which are used for the grep
 */
#define MAX_ARGS 128

/* report error messages */
#define ERROR(fmt, ...) printf("ERROR: " fmt "\n", ##__VA_ARGS__)


/************ global variables *******************/

/* arguments for execvp system call */
char  *proc_args[MAX_PROCS][MAX_ARGS+1];
/* pipes between the forked processes */
int    proc_pipes[MAX_PROCS-1][2];
/* pids of forked processes */
pid_t  proc_pids[MAX_PROCS];
/* number of processes in use */
int    num_procs = 0;
/* pager to use instead of sort/more */
char  *pager;
/************************************************/

/* initialize global variables and create all required pipes
   which will be used later to redirect output between
   processes
*/
static int initialize_resources()
{
  int i;
  int ret;

  /* zero proc_args data structure */
  memset(proc_args, 0, sizeof(proc_args));
  /* zero proc_pipes data structure */
  memset(proc_pipes, 0, sizeof(proc_pipes));
  /* zero proc_pids data structure */
  memset(proc_pids, 0, sizeof(proc_pids));

  for (i=0; i<MAX_PROCS-1; i++) {
    /* create pipes which can be used to redirect stdin and stdout
       between the piped processes
    */
    ret = pipe(proc_pipes[i]);
    if (ret != 0) {
      ERROR("pipe call failed, errno=%s", strerror(errno));
      return ret;
    }
  }
  return 0;
}

/* find the proper pager to use */
int get_pager()
{
  /* when we have environment variable we will use it */
  /* read the PAGER environment variable and use it by default */
  pager = getenv("PAGER");
  if (pager != NULL)
    return 0;

  /* otherwise check that less exists */
  pager = "/usr/bin/less";
  /* make sure the /usr/bin/less path exists */
  if (access(pager, F_OK) == 0)
    return 0;

  /* otherwise check that more exists */
  pager = "/bin/more";
  /* make sure that the /bin/more path exists */
  if (access(pager, F_OK) == 0)
    return 0;

  /* no environment variable and no less/more executable
     in this case we fail
  */
  ERROR("Didn't find pager");
  return -1;
}

/* initialize arguments required to execvp process */
void setup_cmds(int argc, char *argv[])
{
  int i;
  /* count number of processes we execute */
  num_procs = 0;

  /* printenv has no arguments and it is the first process */
  proc_args[num_procs][0] = "printenv";
  num_procs += 1;

  /* if we have command line arguments copy them and use
     them as grep arguments */
  if (argc > 1) {
    proc_args[num_procs][0] = "grep";
    for (i=1; i < argc && i < MAX_ARGS; i++) {
      /* copy arguments from argv to second process args array */
      proc_args[num_procs][i] = argv[i];
    };
    num_procs +=1;
  }

  /* next process is sort and it has zero arguments */
  proc_args[num_procs][0] = "sort";
  num_procs += 1;

  /* next process is pager and it has zero arguments */
  proc_args[num_procs][0] = pager;
  num_procs += 1;
}

/* close all the pipes inside proc_pipes in the range [start, end) */
void close_pipes(int start, int end)
{
  int i;
  for (i=start; i < end; i++) {
    /* close file descriptors for read/write pipes
       starting from "start" and until "end"
    */
    close(proc_pipes[i][0]);
    close(proc_pipes[i][1]);
  }
}

/* fork and execute processes in the pipe */
int execute_pipe()
{
  int i;
  pid_t pid;
  int ret;
  /* fork all processes in loop */
  for (i = 0; i < num_procs; i++) {
    pid = fork();
    if (pid == -1) {
      /* if the fork failes, report error */
      ERROR("pid failed");
      return -1;
    }
    else if (pid == 0) {
      /* we are in child process */
      if (i > 0) {
        /* processes 1,2,... take their stdin from pipe
           process 0 takes stdin from the parent's stdin
         */
        /* close pipes which are not used */
        close_pipes(0, i-1);
        /* duplicate file descriptor 0 (stdin) to pipe's read
           part
        */
        ret = dup2(proc_pipes[i-1][0], 0);
        if (ret == -1)
          exit(-1);
        /* close the write part of the pipe */
        close(proc_pipes[i-1][1]);
      }
      if (i < num_procs - 1) {
        /* child process writes to pipe */
        /* processes 0,1,last -1 write to pipe
           last process writes to parent's stdout
         */
        /* close pipes which are not used */
        close_pipes(i+1, MAX_PROCS-1);
        /* duplicate file descriptor 1 (stdin) to pipe's write
           part
        */
        ret = dup2(proc_pipes[i][1], 1);
        if (ret == -1)
          exit(-1);
        /* close unused read part of the pipe */
        close(proc_pipes[i][0]);
      }

      /* after redirecting stdin/stdout, execute the real program */
      ret = execvp(proc_args[i][0], proc_args[i]);
      if (ret == -1)
        exit(-1);
    }
    else {
      /* store pid for later waitpid call */
      proc_pids[i] = pid;
    }
  }

  return 0;
}

/* wait for all forked processes to exit */
void wait_pids()
{
  int i;
  for (i = 0; i < num_procs; i++) {
    /* iterate over pids and wait to avoid zombi processes */
    if (proc_pids[i] != 0) {
      waitpid(proc_pids[i], NULL, 0);
    }
  }
}

/* main: execute the diagenv pipe */
int main(int argc, char *argv[])
{
  int ret = 0;

  /* initalize variables and allocate pipes */
  ret = initialize_resources();
  if (ret != 0) {
    return ret;
  }

  /* determine what pager to use */
  ret = get_pager();
  if (ret != 0) {
    return ret;
  }

  /* set command arguments */
  setup_cmds(argc, argv);
  /* execute all processes and connect them with pipes */
  ret = execute_pipe();
  /* close all pipes since they are used in the forked processes */
  close_pipes(0, MAX_PROCS-1);
  /* wait for all children processes to terminate */
  wait_pids();

  return ret;
}
