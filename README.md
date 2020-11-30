# Smash
The following code implements a small bash, handling built-in commands, external and special commands, and also signals handling.

# Built-in Commands
1.<ins> chprompt command: </ins>  chprompt <new-prompt> <br />
chprompt command will allow the user to change the prompt displayed by the smash while waiting for the next command. <br />
 
2.<ins>ls command: </ins><br />
ls command will allow the user to display the current files and directories. <br />
 
3.<ins> showpid command: </ins><br />
showpid command prints the smash pid. <br />
 
4.<ins> pwd command: </ins><br />
pwd prints the full path of the current working directory. <br />
  
5.<ins>cd command: </ins> cd <new-path> <br />
Change directory (cd) command receives a single argument <path> that describes the relative or full path to change current working directory to it. <br />
 
6.<ins>jobs command: </ins><br />
jobs command prints the jobs list which contains: <br />
-unfinished jobs (which are running in the background). <br />
-stopped jobs (which were stopped by pressing Ctrl+Z while they are running). <br />
  
7.<ins> kill command: </ins> kill -<signum> <job-id>
Kill command sends a signal which its number specified by <signum> to the job which its sequence ID in jobs list is <job-id> (same as job-id in jobs command). and prints a
message reporting that the specified signal was sent to the specified job. <br />
 
8.<ins> fg command: </ins>fg <job-id> <br />
fg command brings a stopped process or a process that runs in the background to the foreground. <br />
 
9.<ins> bg command: </ins> bg <job-id> <br />
bg command resumes one of the stopped processes in the background. <br />
  
10.<ins> quit command: </ins> quit [kill] <br />
quit command exits the smash. If kill argument was specified (which is optional) then smash should kill all of its unfinished and stopped jobs before exiting. <br />
 
 # Enternal Commands
External command is any command that is not a built-in command or a special command (see special commands bellow ).
The smash executes the external command and wait until the external command is executed.

# Special Commands
1. <ins>Pipes and IO redirection</ins>
2. <ins>timeout command</ins><br />

# Signals Handling
1.<ins> Ctrl-c:</ins><br />
Kills the process in foreground. <br />
2. <ins> Ctrl-z:</ins><br />
Stops the process in foreground. <br />
3. 
