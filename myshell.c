#include <unistd.h>
#include <sys/wait.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <errno.h>
#include <dirent.h>

#define BUFFSIZE 1055 // the maximam number of command character
#define MAX_CMD 200   // the maximum number of commands

typedef struct Job
{
    pid_t pid;
    int jid;          // job id
    char *command;    // matching command
    int has_amper;    // whether have the &
    struct Job *next; // next job
} Job;

// global variable
int argc;
char *argv[MAX_CMD];
int n; // record the size of history
char his[MAX_CMD][BUFFSIZE];
char input[BUFFSIZE]; // receive the input string from keyboard or file
char backup1[BUFFSIZE];
char backup[BUFFSIZE];
char Home_dir[180]; // record home directory
int has_amper;      // judge a input string whether have &
typedef void handler_t(int);
pid_t cpid;
Job *first_job; // using the single linked list to store all jobs,this is the head node.

// All the functions I used  list below
Job *add_job(pid_t pid, int jid, char *command, int has_amper); // add a new job
void delete_job(Job *job);                                      // delete a specific job
Job *getjobFromPid(pid_t pid);                                  // retreive a job from the pid
Job *getJobFromJid(int jid);                                    // retreive a job from the job id.
int getJid();                                                   // get the current job id and assign it to the new job
char getStatus(pid_t me);                                       // get the status of the jobs
void listJob();                                                 // jobs function, list all jobs in the job list
handler_t *Signal(int signum, handler_t *handler);              // using the sigaction to handle the signal
void sigchld_handler(int sig);                                  // handle the SIGCHLD signal
void sigstp_handler(int sig);                                   // handle the SIGTSTP signal
void parse(char *input);                                        // parse the input string to seperate part
int has_pipe();                                                 // check the input string whether have the pipeline
void wait_for_job(Job *j);                                      // parent process wait for the children process to finish
void do_bg();                                                   // backgroud function
void do_fg();                                                   // foreground function
void do_kill();                                                 // kill command
void do_com(int argc, char *argv[MAX_CMD]);                     // do some command after parsing the input string
int comOfPipe(char *input);                                     // pipeline function
void printHis();                                                // print the history
int callCd();                                                   // cd function

// add a new job
Job *add_job(pid_t pid, int jid, char *command, int has_amper)
{
    Job *j = malloc(sizeof(Job));

    j->pid = pid;
    j->jid = jid;
    j->command = malloc(10000 * sizeof(char));
    j->has_amper = has_amper;
    strcpy(j->command, command);
    j->next = NULL;

    // if the first job is null,we set the first_job as the new job
    if (!first_job)
    {
        first_job = j;
        return j;
    }

    // we will find the last job and let the new job be the last job
    Job *t = first_job;
    while (t->next)
    {
        t = t->next;
    }

    t->next = j;

    return j;
}

// delete the specific job
void delete_job(Job *job)
{
    if (first_job == job)
    {
        first_job = first_job->next;
        return;
    }

    Job *t = first_job;

    while (t->next != NULL && t->next != job)
    {
        t = t->next;
    }

    // judge whether the specific job is in job list
    if (t->next != NULL)
    {
        t->next = t->next->next;
    }

    free(job);
}

// get a job according to the specific pid
Job *getjobFromPid(pid_t pid)
{
    Job *t = first_job;

    while (t && t->pid != pid)
    {
        t = t->next;
    }

    return t;
}

// get a job according to the specific job id
Job *getJobFromJid(int jid)
{
    Job *t = first_job;
    while (t && t->jid != jid)
    {
        t = t->next;
    }

    return t;
}

// we will get the maximum job id in job list,and return the max job id+1.
int getJid()
{
    if (!first_job)
    {
        return 1;
    }

    int max1 = -1;
    for (Job *j = first_job; j; j = j->next)
    {
        if (j->jid > max1)
        {
            max1 = j->jid;
        }
    }

    return max1 + 1;
}

// referencing tutorial status.c code,using for finding the status of process.
char getStatus(pid_t me)
{
    int c;
    char pidtext[10];
    char procfilename[100];
    FILE *procfile;

    sprintf(pidtext, "%d", me);
    strcpy(procfilename, "/proc/");
    strcat(procfilename, pidtext);
    strcat(procfilename, "/stat");
    procfile = fopen(procfilename, "r");
    if (procfile == NULL)
    {
        return 'D';
    }

    do
    {
        c = fgetc(procfile);

    } while (c != ')');

    fgetc(procfile);
    c = fgetc(procfile);
    return c;
}

// jobs command,list all jobs
void listJob()
{
    for (Job *j = first_job; j; j = j->next)
    {

        char *string[5] = {"Zombie", "Running", "Sleeping", "Stopped", "Done"};
        char *str;

        // list all possible outcomes
        char c = getStatus(j->pid);
        if (c == 'Z')
        {
            str = string[0];
        }
        else if (c == 'T')
        {
            str = string[3];
        }
        else if (c == 'R')
        {
            str = string[1];
        }
        else if (c == 'S')
        {
            str = string[2];
        }
        else
        {
            str = string[4];
        }

        printf("[%d] ", j->jid);
        printf(" <%s>   ", str);
        printf("  %s\n", j->command);
    }
}

// wrap the sigaction to handle the signal
handler_t *Signal(int signum, handler_t *handler)
{
    struct sigaction action, old_action;

    action.sa_handler = handler;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_RESTART;

    if (sigaction(signum, &action, &old_action) < 0)
    {
        printf("Signal error");
    }
    return (old_action.sa_handler);
}

// handle SIGCHLD signal
void sigchld_handler(int sig)
{
    pid_t pid;
    int status;
    sigset_t mask;

    sigemptyset(&mask);
    sigaddset(&mask, SIGCHLD);

    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    {
        Job *j = getjobFromPid(pid);
        printf("\n[%d] <Done>     %s\n", j->jid, j->command);
        delete_job(j);
    }
}

// handle SIGTSTP signal
void sigstp_handler(int sig)
{
    if (sig == SIGTSTP)
    {
        // use the kill function send the signal
        kill(-cpid, sig);
    }
}

// parsing the input string
void parse(char *input)
{

    // backup1 store the origin input string,and we will use for the command in job.
    strcpy(backup1, input);
    for (int i = 0; input[i] != '\0'; i++)
    {
        if (input[i] == '&')
        {
            has_amper = 1;
            input[i] = '\0';
            input[i - 1] = '\0';
            break;
        }
    }

    // backup is used for if the input string have the &, we will delete the & from the input string,and direct execute the command
    // without the &
    strcpy(backup, input);
    int t = 0;
    argc = 0;

    for (int i = 0; input[i] != '\0'; i++)
    {
        if (t == 0 && !isspace(input[i]))
        {
            argv[argc++] = input + i;
            t = 1;
        }
        else if (t == 1 && isspace(input[i]))
        {
            t = 0;
            input[i] = '\0';
        }
    }

    argv[argc] = NULL;

    // record the command in history
    if (strcmp(argv[0], "history") == 0 || strcmp(argv[0], "h") == 0)
    {
        if (argc == 2)
        {
            return;
        }
    }

    strcpy(his[n++], backup1);
}

// check whether have pipeline
int has_pipe()
{
    for (int i = 0; argv[i] != NULL; i++)
    {
        if (strcmp(argv[i], "|") == 0)
        {
            return 1;
        }
    }

    return 0;
}

// do backgroud function
void do_bg()
{
    if (argc > 2)
    {
        printf("error input format\n");
        return;
    }

    Job *t = NULL;
    if (argc == 1)
    {
        for (Job *j = first_job; j; j = j->next)
        {
            if (getStatus(j->pid) == 'T')
            {
                t = j;
            }
        }
    }
    else
    {
        int a = atoi(argv[1]);
        Job *j;
        j = getJobFromJid(a);
        if (!j)
        {
            t = NULL;
        }
        else
        {
            if (j->has_amper)
            {
                t = NULL;
            }
            else
            {
                t = j;
            }
        }
    }

    if (!t)
    {
        printf("No stopped job to do or no matching stopped job with that jid\n");
        return;
    }
    else
    {
        kill(-(t->pid), SIGCONT);
        char *st = " &";
        char str[BUFFSIZE];
        strcpy(str, t->command);
        strcat(str, st);
        printf("%s\n", str);

        return;
    }
}

// do foreground function
void do_fg()
{

    if (argc > 2)
    {
        printf("error input format\n");
        return;
    }

    Job *t = NULL;
    if (argc == 1)
    {
        for (Job *j = first_job; j; j = j->next)
        {
            if (getStatus(j->pid) == 'T')
            {
                t = j;
            }
        }
    }
    else
    {
        int a = atoi(argv[1]);
        Job *j;
        j = getJobFromJid(a);

        if (!j)
        {
            t = NULL;
        }
        else
        {
            if (j->has_amper)
            {
                t = NULL;
            }
            else
            {
                t = j;
            }
        }
    }

    if (!t)
    {
        printf("No stopped job to do or no matching stopped job with that jid\n");
        return;
    }
    else
    {
        printf("%s\n", t->command);
        // send the SIGCONT signal to let the matching pid process continue running.
        kill(-(t->pid), SIGCONT);

        // we need to wait for children process to finish
        wait_for_job(t);
    }

    return;
}

// do kill function,kill the stopped process in job list
void do_kill()
{
    if (argc > 2)
    {
        printf("error input format\n");
        return;
    }

    Job *t = NULL;
    if (argc == 1)
    {
        for (Job *j = first_job; j; j = j->next)
        {
            if (getStatus(j->pid) == 'T')
            {
                t = j;
            }
        }
    }
    else
    {
        int a = atoi(argv[1]);
        Job *j;
        j = getJobFromJid(a);

        if (!j)
        {
            t = NULL;
        }
        else
        {
            if (j->has_amper)
            {
                t = NULL;
            }
            else
            {
                t = j;
            }
        }
    }

    if (!t)
    {
        printf("No stopped job to kill or no matching stopped job with that jid\n");
        return;
    }
    else
    {
        printf("[%d]   KILLED      %s\n", t->jid, t->command);
        // send the SIGKILL signal to kill the matching the process according to pid
        kill(-(t->pid), SIGKILL);
        // wait for the children process to finish
        wait_for_job(t);
    }

    return;
}

// do command
void do_com(int argc, char *argv[MAX_CMD])
{

    if (strcmp(argv[0], "history") == 0 || strcmp(argv[0], "h") == 0)
    {
        printHis();
    }
    else if (strcmp(argv[0], "cd") == 0)
    {
        int ans = callCd();
        if (!ans)
            printf("cd command input error\n");
    }
    else if (strcmp(argv[0], "exit") == 0 || strcmp(argv[0], "quit") == 0)
    {
        exit(0);
    }
    else if (strcmp(argv[0], "jobs") == 0)
    {
        listJob();
    }
    else if (strcmp(argv[0], "fg") == 0)
    {
        do_fg();
    }
    else if (strcmp(argv[0], "bg") == 0)
    {
        do_bg();
    }
    else if (strcmp(argv[0], "kill") == 0)
    {
        do_kill();
    }
    else
    {

        cpid = fork();

        if (cpid == -1)
        {
            perror("error:");
            exit(1);
        }
        else if (cpid == 0)
        {

            // set the different process group for the parent and children process,the aims is when press ctrl+z will only stopped the
            // children process
            setpgid(0, 0);

            if (has_pipe())
            {
                comOfPipe(backup);
            }
            else
            {
                execvp(argv[0], argv);
                printf("The command '%s' is not recognised\n", argv[0]);
                exit(1);
            }
        }
        else

        {

            int jid = getJid();

            Job *job = add_job(cpid, jid, backup1, has_amper);

            // has & means we will run at the background
            if (has_amper)
            {
                printf("[%d]      %d\n", jid, cpid);
                // ignore the stop signal. means the ctrl+z is not affect the background process
                Signal(SIGTSTP, SIG_IGN);
            }
            else
            {

                // wait for the children process finished
                wait_for_job(job);
            }
        }
    }
}

// wait for the children process finished
void wait_for_job(Job *j)
{
    int status;
    if (waitpid(j->pid, &status, WUNTRACED) > 0)
    {
        // this means the children process was stopped
        if (WIFSTOPPED(status))
        {
            WSTOPSIG(status);
            printf("[%d]   STOPPED       %s\n", j->jid, j->command);
        }
        else
        {
            // not stopped by ctrl+z, exit normally. we delete this job
            delete_job(j);
        }
    }
}

// print the history and execute the command if have the parameter
void printHis()
{
    if (argc > 2)
    {
        printf("wrong input command\n");
    }
    else
    {
        if (argv[1] == NULL)
        {
            if (n < 10)
            {
                for (int i = 0; i < n; i++)
                {
                    printf("%d:%s\n", i + 1, his[i]);
                }
            }
            else
            {
                for (int i = n - 10; i < n; i++)
                {
                    printf("%d:%s\n", i + 1, his[i]);
                }
            }
        }
        else
        {
            if (strspn(argv[1], "0123456789") == strlen(argv[1]))
            {
                int num = atoi(argv[1]);
                if (num > n)
                {
                    printf("input integer exceed the total number of history\n");
                }
                else
                {
                    char str[BUFFSIZE];
                    strcpy(str, his[num - 1]);
                    parse(str);
                    do_com(argc, argv);
                }
            }
            else
            {
                printf("wrong input format,the second parameter must be integer\n");
            }
        }
    }
}

// execute the cd command
int callCd()
{

    if (argc > 2)
    {
        return 0;
    }

    if (argv[1] == NULL)
    {
        chdir(Home_dir);
        return 1;
    }
    else
    {
        if (access(argv[1], 0))
        {
            printf("cd: %s: No such file or directory\n", argv[1]);
        }
        else
        {
            chdir(argv[1]);
        }

        return 1;
    }
}

// execute the pipeline command
int comOfPipe(char *input)
{
    int num = 0;
    for (int i = 0; i < strlen(input); i++)
    {
        if (input[i] == '|')
        {
            num++;
        }
    }

    char first[100];
    char second[100];
    int idx = 0;
    for (int i = strlen(input); i >= 0; i--)
    {
        if (input[i] == '|')
        {
            idx = i;
            break;
        }
    }

    memset(first, 0, sizeof first);
    memset(second, 0, sizeof second);
    for (int i = 0; i < idx - 1; i++)
    {
        first[i] = input[i];
    }

    for (int i = 0, j = idx + 2; input[j] != '\0'; j++, i++)
    {
        second[i] = input[j];
    }

    int pd[2];
    pid_t pid;

    if (pipe(pd) < 0)
    {
        perror("pipe()");
        return 0;
    }

    pid = fork();

    if (pid < 0)
    {
        perror("fork()");
        return 0;
    }

    if (pid == 0)
    {
        close(pd[0]);
        dup2(pd[1], STDOUT_FILENO);

        int t = 1;
        for (int i = 0; first[i] != '\0'; i++)
        {
            if (first[i] == '|')
            {
                t = 0;
                break;
            }
        }

        if (t)
        {
            parse(first);
            execvp(argv[0], argv);
            printf("The command '%s' is not recognised\n", argv[0]);
            exit(1);
        }
        else
        {
            // using recursive to solve the multiple pipeline
            comOfPipe(first);
        }

        close(pd[1]);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
        close(pd[1]);
        dup2(pd[0], STDIN_FILENO);
        parse(second);
        close(pd[0]);
        execvp(argv[0], argv);
        printf("The command '%s' is not recognised\n", argv[0]);
        exit(1);
    }

    return 1;
}

int main()
{

    // get the home directory
    getcwd(Home_dir, sizeof Home_dir);
    n = 0;

    while (1)
    {

        // receive the stopped signal when type the ctrl+z
        Signal(SIGTSTP, sigstp_handler);
        printf("ash> ");

        // initialise
        memset(input, 0, sizeof input);
        memset(backup, 0, sizeof backup);
        memset(backup1, 0, sizeof backup1);

        // if read the end of file we will exit;
        if (feof(stdin))
        {
            exit(0);
        }

        // receive the SIGCHLD signal for the children process which run the background command,we will handle the
        // signal and  kill the children process in case becoming the Zombie process
        Signal(SIGCHLD, sigchld_handler);
        // read the input
        fgets(input, BUFFSIZE, stdin);
        // avoid the "\n";
        input[strlen(input) - 1] = '\0';

        // judge the input from the keyboard or file
        if (!isatty(STDIN_FILENO))
        {
            printf("%s\n", input);
        }

        if (strlen(input) == 0)
        {
            continue;
        }

        has_amper = 0;
        parse(input);
        do_com(argc, argv);
    }

    return 0;
}
