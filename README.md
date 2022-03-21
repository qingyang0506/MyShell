# MyShell

## Introduction
I  write a shell in c programme language. Implement multiple shell commands such as the cd, history, pipeline, jobs, background, fg,bg etc. These commands are not as powerful as the shell commands. I also make some changes for these command for my own shell.

## Shell command Function
1. You can execute all the external shell commands from my shell. 
2. For the history command, Print history or h can list the lastest 10 commands. And every command have it own id.  Print a number followd by h or history, it will execute the command according to the id.
3. For the jobs command, you will see all stopped ,sleeping or running jobs, companied with their job id and the command string.
4. Enter the command with & will let this job running at the background,we can use jobs command to check its status. 
5. Typing the ctrl+z will stopped the running job at foreground, we can use fg command to avoke the job to run at foreground, bg for background or use the kill command to kill the jobs. If there is no parameters it will work for the lastest stopped job.
6. You can print exit or quit to exit the shell

## How to Run
It is easy for running, Just compile the myshell.c  to myshell. Then use ./ to run it

    complie: gcc -o myshell myshell.c
    running: ./myshell
   
   
