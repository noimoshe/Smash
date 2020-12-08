# Smash
The following code implements a small shell, handling built-in commands, external and special commands. Also includes signals handling.

# Built-in Commands
1.**<ins> chprompt command: </ins>**  chprompt \[new-prompt\] <br /> 
chprompt command will allow the user to change the prompt displayed by the smash while waiting for the next command. <br />
 
2.**<ins>ls command: </ins>**  ls<br />
ls command will allow the user to display the current files and directories. <br />
 
3.**<ins> showpid command: </ins>**  showpid<br />
showpid command prints the smash pid. <br />
 
4.**<ins> pwd command: </ins>**  ped<br />
pwd prints the full path of the current working directory. <br />
  
5.**<ins>cd command: </ins>** cd \[new-path\] <br />
Change directory (cd) command receives a single argument <path> that describes the relative or full path to change current working directory to it. <br />
 
6.**<ins>jobs command: </ins>** jobs<br />
jobs command prints the jobs list which contains: <br />
-unfinished jobs (which are running in the background). <br />
-stopped jobs (which were stopped by pressing Ctrl+Z while they are running). <br />
  
7.**<ins> kill command: </ins>** kill -\[signum\] \[job-id\]
Kill command sends a signal which its number specified by <signum> to the job which its sequence ID in jobs list is <job-id> (same as job-id in jobs command). and prints a
message reporting that the specified signal was sent to the specified job. <br />
 
8.**<ins> fg command: </ins>** fg \[job-id\] <br />
fg command brings a stopped process or a process that runs in the background to the foreground. <br />
 
9.**<ins> bg command: </ins>** bg \[job-id\] <br />
bg command resumes one of the stopped processes in the background. <br />
  
10.**<ins> quit command: </ins>** quit [kill] <br />
Quit command exits the smash. If kill argument was specified (which is optional) then smash should kill all of its unfinished and stopped jobs before exiting. <br />
 
 # External Commands
External command is any command that is not a built-in command or a special command (see special commands bellow ).
The smash executes the external command and wait until the external command is executed.

# Special Commands
1.**<ins>Pipes and IO redirection</ins>**
Each typed command could have up to one character of pipe or IO redirection. IO redirection characters which are supported: “>” and “>>”. <br/>
Pipe characters that are supported: “|” and “|&”.<br/>
2.**<ins>timeout command:</ins>** timeout [duration] [command]<br />
Sets an alarm for ‘duration’ seconds, and runs the given ‘command’ as though it was given to the smash directly, and when the time is up it shall send a SIGKILL to the given
command’s process (unless it’s the smash itself).

# Signals Handling
1.**<ins> Ctrl-c:</ins>**<br />
Kills the process in foreground. <br />
2.**<ins> Ctrl-z:</ins>**<br />
Stops the process in foreground. <br />
3.**<ins> alarm signal: </ins>**<br />
When receiving a SIG_ALRM search which command caused the alarm, send a SIGKILL to its process.
