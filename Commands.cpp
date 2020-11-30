#include <unistd.h>
#include <string.h>
#include <iostream>
#include <vector>
#include <sstream>
#include <sys/wait.h>
#include <iomanip>
#include "Commands.h"
#include <string>
#include <stdio.h>
#include <sys/types.h>
#include <signal.h>
#include <dirent.h>
#include <fcntl.h>

using namespace std;

char* old_path = nullptr;
bool changed_path = false;
pid_t cur_fg_pid=-1; // a global parameter which contains the pid of current foreground command (-1 if there is no such command)
char* cur_fg_cmd = new char[80];

const std::string WHITESPACE = " \n\r\t\f\v";

#if 0
#define FUNC_ENTRY()  \
  cerr << __PRETTY_FUNCTION__ << " --> " << endl;

#define FUNC_EXIT()  \
  cerr << __PRETTY_FUNCTION__ << " <-- " << endl;
#else
#define FUNC_ENTRY()
#define FUNC_EXIT()
#endif

#define DEBUG_PRINT cerr << "DEBUG: "

#define EXEC(path, arg) \
  execvp((path), (arg));

string _ltrim(const std::string& s)
{
  size_t start = s.find_first_not_of(WHITESPACE);
  return (start == std::string::npos) ? "" : s.substr(start);
}

string _rtrim(const std::string& s)
{
  size_t end = s.find_last_not_of(WHITESPACE);
  return (end == std::string::npos) ? "" : s.substr(0, end + 1);
}

string _trim(const std::string& s)
{
  return _rtrim(_ltrim(s));
}

int _parseCommandLine(const char* cmd_line, char** args) {
  FUNC_ENTRY()
  int i = 0;
  std::istringstream iss(_trim(string(cmd_line)).c_str());
  for(std::string s; iss >> s; ) {
    args[i] = (char*)malloc(s.length()+1);
    memset(args[i], 0, s.length()+1);
    strcpy(args[i], s.c_str());
    args[++i] = NULL;
  }
  return i;

  FUNC_EXIT()
}

bool _isBackgroundComamnd(const char* cmd_line) {
  const string str(cmd_line);
  return str[str.find_last_not_of(WHITESPACE)] == '&';
}

void _removeBackgroundSign(char* cmd_line) {
  const string str(cmd_line);
  // find last character other than spaces
  unsigned int idx = str.find_last_not_of(WHITESPACE);
  // if all characters are spaces then return
  if (idx == string::npos) {
    return;
  }
  // if the command line does not end with & then return
  if (cmd_line[idx] != '&') {
    return;
  }
  // replace the & (background sign) with space and then remove all tailing spaces.
  cmd_line[idx] = ' ';
  // truncate the command line string up to the last non-space character
  cmd_line[str.find_last_not_of(WHITESPACE, idx) + 1] = 0;
}



Command::Command(const char *cmd_line, JobsList* jobsList) {
    this->cmd_line = new char[strlen(cmd_line)+1];
    strcpy(this->cmd_line, cmd_line);
    this->jobsList = jobsList;
}

Command::~Command() noexcept {
    delete this->cmd_line;
}

void PipeCommand::PipeCommand::execute() {
    this->jobsList->removeFinishedJobs();
    char **args = new char *[20];
    int numArgs = _parseCommandLine(this->cmd_line, args);
    SmallShell& smash = SmallShell::getInstance();
    std::string cmd_s = string(cmd_line);

    int first = cmd_s.find("|");
    int second = cmd_s.find("&");
    bool point = false;
    if(first == second-1){
        point = true;
    }
    if (point) {
        int breakpoint = cmd_s.find("|");
        std::string command = _trim(cmd_s.substr(0, breakpoint));
        std::string file = _trim(cmd_s.substr(breakpoint + 2, cmd_s.length() + 1));
        char *c_command = new char[80];
        strcpy(c_command, command.c_str());
        char *c_command2 = new char[80];
        strcpy(c_command2, file.c_str());

        bool is_bi1 = smash.isBuiltIn(c_command);
        bool is_bi2 = smash.isBuiltIn(c_command2);
        if(!is_bi1 && !is_bi2){
            ExternalCommand* cmd = new ExternalCommand(this->cmd_line, this->jobsList, false);
            smash.commands.push_back(cmd);
            cmd->execute();

            delete c_command;
            delete c_command2;
            delete args;
            return;
        }

        if(is_bi2){
            smash.executeCommand(c_command2);
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }

        int fd[2];
        int result=pipe(fd);
        if(result==-1) {
            perror("smash error: pipe failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }

        pid_t pid = fork();
        if (pid == 0) {
            setpgrp();
            if(dup2(fd[0], 0)<0){
                perror("smash error: dup2 failed");
                delete c_command;
                delete c_command2;
                delete args;
                return;
            }
            if(close(fd[0]) < 0){
                perror("smash error: close failed");
                delete c_command;
                delete c_command2;
                delete args;
                return;
            }
            if(close(fd[1]) <0){
                perror("smash error: close failed");
                delete c_command;
                delete c_command2;
                delete args;
                return;
            }
            execl("/bin/bash", "/bin/bash", "-c", c_command2, NULL);
        }

        int old_stderr = dup(2);
        if(old_stderr < 0){
            perror("smash error: dup failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        if(dup2(fd[1], 2) < 0){
            perror("smash error: dup2 failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        if(close(fd[0])<0){
            perror("smash error: close failed");
            dup2(old_stderr, 2);
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        if(close(fd[1])<0){
            perror("smash error: close failed");
            dup2(old_stderr, 2);
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        smash.executeCommand(c_command);
        dup2(old_stderr, 2);

        result=wait(NULL);
        if (result==-1){
            perror("smash error: wait failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }//wait failed


        delete c_command;
        delete c_command2;
        delete args;
        return;

    } else {
        int breakpoint = cmd_s.find("|");
        std::string command = _trim(cmd_s.substr(0, breakpoint));
        std::string file = _trim(cmd_s.substr(breakpoint + 1, cmd_s.length() + 1));
        char *c_command = new char[80];
        strcpy(c_command, command.c_str());
        char *c_command2 = new char[80];
        strcpy(c_command2, file.c_str());

        bool is_bi1 = smash.isBuiltIn(c_command);
        bool is_bi2 = smash.isBuiltIn(c_command2);
        if(!is_bi1 && !is_bi2){
            ExternalCommand* cmd = new ExternalCommand(this->cmd_line, this->jobsList, false);
            smash.commands.push_back(cmd);
            cmd->execute();

            delete c_command;
            delete c_command2;
            delete args;
            return;
        }

        if(is_bi2){
            smash.executeCommand(c_command2);
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }

        int fd[2];
        int result=pipe(fd);
        if(result==-1){
            perror("smash error: pipe failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }

        pid_t pid = fork();
        if (pid == 0) {
            setpgrp();
            if(dup2(fd[0], 0)<0){
                perror("smash error: dup2 failed");
                delete c_command;
                delete c_command2;
                delete args;
                return;
            }
            if(close(fd[0]) < 0){
                perror("smash error: close failed");
                delete c_command;
                delete c_command2;
                delete args;
                return;
            }
            if(close(fd[1]) <0){
                perror("smash error: close failed");
                delete c_command;
                delete c_command2;
                delete args;
                return;
            }
            execl("/bin/bash", "/bin/bash", "-c", c_command2, NULL);
        }

        int old_stdout = dup(1);
        if(old_stdout < 0){
            perror("smash error: dup failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        if(dup2(fd[1], 1) < 0){
            perror("smash error: dup2 failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        if(close(fd[0])<0){
            perror("smash error: close failed");
            dup2(old_stdout, 1);
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        if(close(fd[1])<0){
            perror("smash error: close failed");
            dup2(old_stdout, 1);
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }
        smash.executeCommand(c_command);
        dup2(old_stdout, 1);

        result=wait(NULL);
        if (result==-1){
            perror("smash error: wait failed");
            delete c_command;
            delete c_command2;
            delete args;
            return;
        }//wait failed


        delete c_command;
        delete c_command2;
        delete args;
        return;
    }
}

void RedirectionCommand::RedirectionCommand::execute() {
    this->jobsList->removeFinishedJobs();
    char** args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line ,args);
    SmallShell& smash = SmallShell::getInstance();
    std::string cmd_s = string(cmd_line);
    int point = cmd_s.find(">>");
    if(point != -1){
        std::string command = cmd_s.substr(0,point);
        std::string file = cmd_s.substr(point+2, cmd_s.length()+1);
        const auto strBegin = file.find_first_not_of(" \t");
        const auto strEnd = file.find_last_not_of(" \t");
        const auto strRange = strEnd - strBegin + 1;
        file = file.substr(strBegin, strRange);
        if(_isBackgroundComamnd(this->cmd_line)){
            command += "&";
        }
        char* c_command = new char[80];
        strcpy(c_command, command.c_str());
        char* c_file = new char[80];
        strcpy(c_file, file.c_str());
        _removeBackgroundSign(c_file);
        std::string fixed = _trim(c_file);
        strcpy(c_file, fixed.c_str());
        bool is_bi = smash.isBuiltIn(c_command);
        //if(is_bi){
        int new_file = open(c_file, O_WRONLY | O_CREAT | O_APPEND, 0666);
        if(new_file < 0){
            perror("smash error: open failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        int old_stdout = dup(1);
        if(old_stdout < 0){
            perror("smash error: dup failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        if( dup2(new_file, 1) < 0){
            perror("smash error: dup2 failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        if(is_bi)
            smash.executeCommand(c_command);
        else {
            ExternalCommand* cmd = new ExternalCommand(c_command, this->jobsList, false);
            cmd->execute();
            smash.commands.push_back(cmd);
        }
        int ret_val = close(new_file);
        if(ret_val < 0){
            perror("smash error: close failed");
            dup2(old_stdout, 1);
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        if(dup2(old_stdout, 1) < 0){
            perror("smash error: dup2 failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        //}
        delete c_command;
        delete c_file;
        delete args;
        return;
    }
    else {
        point = cmd_s.find(">");
        std::string command = cmd_s.substr(0,point);
        std::string file = cmd_s.substr(point+1, cmd_s.length()+1);
        char* c_command = new char[80];
        if(_isBackgroundComamnd(this->cmd_line)){
            command += "&";
        }
        strcpy(c_command, command.c_str());
        char* c_file = new char[80];
        const auto strBegin = file.find_first_not_of(" \t");
        const auto strEnd = file.find_last_not_of(" \t");
        const auto strRange = strEnd - strBegin + 1;
        file = file.substr(strBegin, strRange);
        strcpy(c_file, file.c_str());
        _removeBackgroundSign(c_file);
        std::string fixed = _trim(c_file);
        strcpy(c_file, fixed.c_str());
        bool is_bi = smash.isBuiltIn(c_command);
        int new_file = open(c_file, O_WRONLY | O_CREAT | O_TRUNC, 0666);
        if(new_file < 0){
            perror("smash error: open failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        int old_stdout = dup(1);
        if(old_stdout < 0){
            perror("smash error: dup failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        if( dup2(new_file, 1) < 0){
            perror("smash error: dup2 failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        if(is_bi)
            smash.executeCommand(c_command);
        else {
            ExternalCommand* cmd = new ExternalCommand(c_command, this->jobsList, false);
            cmd->execute();
            smash.commands.push_back(cmd);
        }
        int ret_val = close(new_file);
        if(ret_val < 0){
            perror("smash error: close failed");
            dup2(old_stdout, 1);
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        if(dup2(old_stdout, 1) < 0){
            perror("smash error: dup2 failed");
            delete c_command;
            delete c_file;
            delete args;
            return;
        }
        delete c_command;
        delete c_file;
        delete args;
        return;
    }
}

ChangeDirCommand::~ChangeDirCommand() noexcept {

}

void CopyCommand::execute() {
    this->jobsList->removeFinishedJobs();
    bool is_bg = _isBackgroundComamnd(this->cmd_line);
    char** args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line ,args);
    SmallShell& smash = SmallShell::getInstance();

    char* c_file = args[1];
    char* c_file_dest = args[2];
    int new_file = open(c_file, O_RDONLY | O_CREAT, 0666);
    if(new_file < 0){
        perror("smash error: open failed");
        delete args;
        return;
    }
    int new_file_dest = open(c_file_dest, O_RDWR | O_CREAT | O_TRUNC, 0666);
    if(new_file_dest < 0){
        perror("smash error: open failed");
        close(new_file);
        delete args;
        return;
    }

    pid_t pid = fork();
    if(pid == 0){
        int result = 1;
        while(result > 0){
            char* buff = new char[100];
            result = read(new_file, buff, 1);
            if(write(new_file_dest, buff, 1) < 0){
                perror("smash error: write failed");
                close(new_file);
                close(new_file_dest);
                delete args;
                return;
            }
        }
        if(result == -1){
            close(new_file);
            close(new_file_dest);
            perror("smash error: read failed");
            delete args;
            return;
        }
        close(new_file);
        close(new_file_dest);
    }
    else if(pid > 0) {//father
        SmallShell& smash = SmallShell::getInstance();
        if(is_bg){
            ExternalCommand* c = new ExternalCommand(this->cmd_line, this->jobsList, false);
            this->jobsList->addJob(c, false, pid);
        }
        if(!is_bg) {
            //foreground command
            cur_fg_pid=pid; //updating pid of current foreground command
            strcpy(cur_fg_cmd, this->cmd_line);
            int stat;
            if (waitpid(pid, &stat, WUNTRACED) < 0) { //wait failed
                perror("smash error: wait failed");
                cur_fg_pid = -1;
                delete args;
                return;
            } else {
                if (stat == 0) {//means son terminated normally
                    cur_fg_pid=-1; //son terminated -> there is no command in foreground
                    return;
                }
            }
        }
    }
    else {//fork failed
        perror("smash error: fork failed");
        delete args;
        return;
    }

    delete args;
    return;

}

void LSCommand::execute() {
    this->jobsList->removeFinishedJobs();
    struct dirent **namelist;
    int i,n;

    n = scandir(".", &namelist, 0, alphasort);
    if (n < 0)
        perror("smash error: scandir failed");
    else {
        for (i = 2; i < n; i++) {
            std:: cout << namelist[i]->d_name << endl;
            free(namelist[i]);
        }
    }
    free(namelist);
}

void ChangeDirCommand::execute(){
    this->jobsList->removeFinishedJobs();
    char** args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line ,args);
    if(numArgs > 2) {
        std::cout << "smash error: cd: too many arguments" << endl;
        delete args;
        return;
    }
    if(strcmp(args[1],"-") == 0) {
        if(changed_path == false){
            std::cout << "smash error: cd: OLDPWD not set" << endl;
            delete args;
            return;
        }
        else {
            char* temp = get_current_dir_name();
            if(chdir(old_path) < 0){
                perror("smash error: chdir failed");
            }
            old_path = temp;
            changed_path = true;
            delete args;
            return;
        }
    }
    else if(strcmp(args[1], "..") == 0) {
        old_path = get_current_dir_name();
        changed_path = true;
        string cmd_s = string(get_current_dir_name());
        int end = cmd_s.find_last_of("/");
        cmd_s = cmd_s.substr(0, end + 1);
        char cut_cmd_line[cmd_s.length()];
        strcpy(cut_cmd_line, cmd_s.c_str());
        if(chdir(cut_cmd_line) < 0){
            perror("smash error: chdilr failed");
        }
        delete args;
        return;
    }
    else {
        char* temp = get_current_dir_name();
        int cd_result=chdir(args[1]);
        if (cd_result==-1){
            perror("smash error: chdir failed");
            delete args;
            return;
        } //cd command failed - non-existing path argument
        old_path = temp;
        changed_path = true;
    }
    delete args;
    return;
}

void JobsCommand::execute() {
    jobsList->removeFinishedJobs();
    jobsList->printJobsList();
}



void KillCommand::execute() {
    this->jobsList->removeFinishedJobs();
    char** args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line,args);
    if(numArgs != 3) {
        std::cout<<"smash error: kill: invalid arguments"<<endl;
        delete args;
        return;
    }
    if(args[1][0] != '-') {
        std::cout << "smash error: kill: invalid arguments" << endl;
        delete args;
        return;
    }
    int jobID;
    try{
        jobID=stoi(args[2]);
    }
    catch (std::invalid_argument & ex) {
        std::cout << "smash error: kill: invalid arguments" << endl;
        delete args;
        return;
    }
    char* signum = new char[80];
    strcpy(signum, args[1] + 1);
    int sig;
    try{
        sig=stoi(signum);
    }
    catch (std::invalid_argument & ex) {
        std::cout << "smash error: kill: invalid arguments" << endl;
        delete args;
        return;
    }
    JobsList::JobEntry* job = this->jobsList->getJobById(jobID);
    if(job ==nullptr) {
        std::cout << "smash error: kill: job-id " << jobID << " does not exist" << endl;
        delete args;
        return;
    }
    int result=kill(job->pid, sig);
    if (result==-1){//kill failed
        perror("smash error: kill failed");
    }
    else {
        std::cout << "signal number " << sig << " was sent to pid " << job->pid << endl;
    }
    delete args;
    delete signum;
    return;
}


void ShowPidCommand::execute(){
    this->jobsList->removeFinishedJobs();
    SmallShell& smash = SmallShell::getInstance();
    std::cout << "smash pid is " << smash.pid << endl;
}

void GetCurrDirCommand::execute(){
    this->jobsList->removeFinishedJobs();
    char* name = get_current_dir_name();
    if(name == nullptr){
        perror("smash error: get_current_dir_name failed");
        return;
    }
    std::cout << name << endl;
}


void ForegroundCommand::execute(){
    this->jobsList->removeFinishedJobs();
    char **args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line,args);
    if(numArgs<1 || numArgs>2){
        std::cout<<"smash error: fg: invalid arguments"<<endl;
        delete args;
        return;
    }
    if(numArgs==2){
        if(this->jobsList->maxID <= stoi(args[1]) || stoi(args[1]) <= 0){
            std::cout<<"smash error: fg: job-id "<< args[1] <<" does not exist"<<endl;
            delete args;
            return;
        }
        else if(this->jobsList->getJobById(stoi(args[1]))  ==nullptr){
            std::cout<<"smash error: fg: job-id "<< args[1] <<" does not exist"<<endl;
            delete args;
            return;
        }
    }
    if(numArgs==1){
        if(this->jobsList->maxID==1) { // jobs list is empty
            std::cout << "smash error: fg: jobs list is empty"<<endl;
            delete args;
            return;
        }
    }

    JobsList::JobEntry * jobEntry;
    const char* cmd_line;
    pid_t pid;
    if (numArgs==1){ //"fg" without arguments -> brings cmd in index maxID to foreground
        jobEntry=this->jobsList->jobs->at(this->jobsList->maxID-1);
    }
    else{
        jobEntry=this->jobsList->jobs->at(stoi(args[1]));
    }
    //jobEntry->cmd = nullptr;
    cmd_line=jobEntry->cmd->cmd_line;
    //_removeBackgroundSign(cmd_line);
    pid=jobEntry->pid;

    //printing job's command line + pid:
    std::cout <<cmd_line <<" : "<<pid <<endl;
    //sending SIGCONT to the job:
    int res_kill=kill(pid,SIGCONT);
    if(res_kill==-1){
        perror("smash error: kill failed");
        delete args;
        return;
    }

    cur_fg_pid=pid; //updating pid of current foreground command
    strcpy(cur_fg_cmd, cmd_line);
    this->jobsList->jobsCount--;
    //wait until job will finish:
    int result=waitpid(pid,NULL,WUNTRACED);
    if (result==-1){
        perror("smash error: wait failed");
    }//wait failed

    cur_fg_pid=-1; //process finished -> there is no command in foreground
    delete args;
}

void BackgroundCommand::execute() {
    this->jobsList->removeFinishedJobs();
    char **args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line,args);
    if(numArgs<1 || numArgs>2){
        std::cout<<"smash error: bg: invalid arguments"<<endl;
        delete args;
        return;
    }
    if(numArgs==2){
        if(this->jobsList->maxID <= stoi(args[1]) || stoi(args[1]) <= 0){
            std::cout << "smash error: bg: job-id " << args[1] << " does not exist" << endl;
            delete args;
            return;
        }
        else if(this->jobsList->getJobById(stoi(args[1]))  ==nullptr) {
            std::cout << "smash error: bg: job-id " << args[1] << " does not exist" << endl;
            delete args;
            return;
        }
        if(this->jobsList->getJobById(stoi(args[1]))->isStopped== false) { //job with id arg is not stopped
            std::cout << "smash error: bg: job-id " << args[1] << " is already running in the background" << endl;
            delete args;
            return;
        }
    }

    JobsList::JobEntry * jobEntry;
    const char* cmd_line;
    pid_t pid;
    int job_id;
    if (numArgs==1){ //"bg" without args-> resumes the stopped job with maximal id to background
        jobEntry=this->jobsList->getLastStoppedJob(&job_id);
        if(jobEntry== nullptr){
            std:cout<<"smash error: bg: there is no stopped jobs to resume"<< endl;
            delete args;
            return;
        }
        pid = jobEntry->pid;
    }
    else{
        jobEntry=this->jobsList->jobs->at(stoi(args[1]));
        pid=jobEntry->pid;
    }
    cmd_line=jobEntry->cmd->cmd_line;

    //printing job's command line + pid:
    std::cout<<cmd_line <<" : "<<pid << endl;
    //sending SIGCONT to the job:
    int result=kill(pid,SIGCONT);
    if(result==-1){
        perror("smash error: kill failed");
        delete args;
        return;
    }
    jobEntry->isStopped = false;
    delete args;
}

void QuitCommand::execute() {
    jobsList->removeFinishedJobs();
    char **args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line,args);
    if(numArgs==2){
        if(strcmp(args[1],"kill")==0){
           // jobsList->removeFinishedJobs();
            int maxID=this->jobsList->maxID;
            std::cout<<"smash: sending SIGKILL signal to "<< this->jobsList->jobsCount <<" jobs:"<<endl;
            for(int i=1;i<maxID; i++){
                if(jobsList->jobs->at(i) != nullptr) {
                    const char *cmd = this->jobsList->jobs->at(i)->cmd->cmd_line;
                    int pid = this->jobsList->jobs->at(i)->pid;
                    std::cout << pid << ": " << cmd << endl;
                    int result = kill(pid, SIGKILL);
                    if (result == -1) { //kill failed
                        perror("smash error: kill failed");
                    }
                }
            }
            delete args;
            exit(0);
        }
        else{
            delete args;
            exit(0);
        }
    }
    else{
        delete args;
        exit(0);
    }

}
void ChpromptCommand::execute() {
    this->jobsList->removeFinishedJobs();
    char** args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line ,args);
    if(numArgs == 1){
        *this->name = "smash";
    }
    else {
        *this->name = args[1];
    }
    delete args;
}

JobsList::JobEntry::JobEntry(int jobID,Command* cmd, bool isStopped, pid_t pid){
        this->jobID=jobID;
        this->cmd=cmd;
        this->isStopped=isStopped;
        this->pid = pid;
        this->start = time(nullptr);
}

JobsList::JobsList(){
    this->jobs = new std::vector<JobsList::JobEntry*>;
    this->jobs->push_back(nullptr);
    this->maxID = 1;
    this->jobsCount = 0;
}

JobsList::~JobsList(){
    for(int i = 0; i < jobs->size(); ++i){
        if(jobs->at(i) != nullptr) {
            delete jobs->at(i);
        }
    }
    delete this->jobs;
}

void JobsList::updateJobsCount() {
        int count = 0;
        for(int i = 0; i < this->jobs->size(); ++i) {
            if (this->jobs->at(i) != nullptr) {
                count++;
            }
        }
        this->jobsCount = count;
}

void JobsList::updateMaxId() {
    int count = 0;
    for(int i = 0; i < this->jobs->size(); ++i){
        if(this->jobs->at(i) != nullptr){
            count++;
        }
    }
    if(count == 0){
        this->maxID = 1;
        return;
    }
    int i = this->jobs->size()-1;
    while(i > 0 && this->jobs->at(i) == nullptr) {
        i--;
    }
    if(i == 1 && this->jobs->at(1) !=nullptr){
        this->maxID = 2;
    }
    else{
        this->maxID = i+1;
    }
}


void JobsList::addJob(Command *cmd, bool isStopped, pid_t pid) {
    this->removeFinishedJobs();
    for(int i = 1; i < this->maxID; ++i){
        if(this->jobs->at(i) != nullptr) {
            if (this->jobs->at(i)->pid == pid) {
                this->jobs->at(i)->cmd = cmd;
                this->jobs->at(i)->isStopped = isStopped;
                //this->jobs->at(i)->start = time(nullptr);
                jobsCount++;
                return;
            }
        }
    }
    JobsList::JobEntry* new_job = new JobsList::JobEntry(maxID, cmd, isStopped, pid);
    if(this->jobs->size() > maxID){
        if(this->jobs->at(maxID) == nullptr){
            this->jobs->at(maxID) = new_job;
        }
    }
    else{
        this->jobs->push_back(new_job);
    }
    maxID++;
    jobsCount++;
    //explanation:
    //
}

void JobsList::printJobsList() {
    time_t curr = time(nullptr);
    if(this->maxID == 1) return; //jobs list is empty
    for(int i = 1; i < this->maxID; i++) {
        if (jobs->at(i) != nullptr) {
            if (jobs->at(i)->isStopped) {
                std::cout <<"[" << i << "]" << " " << jobs->at(i)->cmd->cmd_line << " : " << jobs->at(i)->pid << " " <<
                            (difftime(curr,jobs->at(i)->start)) << " secs" << " " << "(stopped)" << endl;
            } else {
                std::cout << "[" << i << "]" << " " << jobs->at(i)->cmd->cmd_line << " : " << jobs->at(i)->pid << " " <<
                          (difftime(curr,jobs->at(i)->start)) << " secs" << endl;
            }
        }
    }
}

void JobsList::removeJobById(int jobId) {
       /* delete this->jobs->at(jobId)->cmd->jobsList;
        delete this->jobs->at(jobId)->cmd; */
    if(this->maxID-1 == jobId){
        //this is the job with the Maximum jobID
        if(maxID-1 == 1){
            this->maxID = 1;
        }
        else {
            int new_max = maxID - 2;
            while (this->jobs->at(new_max) == nullptr && new_max > 0) {
                new_max--;
            }
            if (new_max == 0) {
                this->maxID = 1;
            } else {
                this->maxID = new_max + 1;
            }
        }
    }
    delete this->jobs->at(jobId);
    this->jobs->at(jobId) = nullptr;
    this->jobsCount--;
}

JobsList::JobEntry * JobsList::getLastStoppedJob(int *jobId) {
    for(int i = this->maxID-1; i > 0; i--) {
        if(this->jobs->at(i) != nullptr) {
            if(this->jobs->at(i)->isStopped == true){
                *jobId=this->jobs->at(i)->jobID;
                return this->jobs->at(i);
            }
        }
    }
    return nullptr;
}

void JobsList::removeFinishedJobs(){
    for(int i = 1; i <this->maxID; i++) {
        if(this->jobs->at(i) != nullptr) {
            int res = waitpid(this->jobs->at(i)->pid, NULL, WNOHANG);
            if(res != 0){
                this->removeJobById(i);
            }
        }
    }
    updateMaxId();
    updateJobsCount();
}

JobsList::JobEntry * JobsList::getJobById(int jobId) {
    updateMaxId();
    if(jobId >= this->maxID || jobId <= 0){
        return nullptr;
    }
    return this->jobs->at(jobId);
}

JobsList::JobEntry * JobsList::getLastJob(int *lastJobId) {
    if(maxID == 1)
        return nullptr;
    return this->jobs->at(this->maxID-1);
}

void ExternalCommand::execute() {
    this->jobsList->removeFinishedJobs();
    char* CMDLINE = new char[80];
    strcpy(CMDLINE, this->cmd_line);
    bool is_bg = _isBackgroundComamnd(this->cmd_line);
    if(is_bg){
        _removeBackgroundSign(CMDLINE);
    }
    char arg0[] = "/bin/bash";
    char arg1[] = "-c";
    char* args[] = {arg0, arg1, CMDLINE, nullptr};

    pid_t pid = fork();
    if(pid == 0){ //son
        setpgrp();
        execv("/bin/bash", args);
    }
    else if(pid > 0) {//father
        SmallShell& smash = SmallShell::getInstance();
        if(this->is_timeout == true){
            time_out* new_t = new time_out;
            new_t->total_left = this->duration;
            new_t->pid = pid;
            new_t->start = time(nullptr);
            new_t->duration = this->duration;
            char* cmd_to_timeout = new char[80];
            std::string s = "timeout ";
            int d = this->duration;
            s+= std::to_string(d);
            s+= " ";
            s+= _trim(this->cmd_line);
            strcpy(cmd_to_timeout, s.c_str());
            new_t->cmd_line = cmd_to_timeout;
            smash.timeout_list.push_back(new_t);
        }
        if(is_bg){
            if(this->is_timeout){
                char* cmd_to_timeout = new char[80];
                std::string s = "timeout ";
                int d = this->duration;
                s+= std::to_string(d);
                s+= " ";
                s+= _trim(this->cmd_line);
                strcpy(cmd_to_timeout, s.c_str());
                ExternalCommand* c = new ExternalCommand(cmd_to_timeout, this->jobsList, false);
                this->jobsList->addJob(c, false, pid);
            }
            else{
                ExternalCommand* c = new ExternalCommand(this->cmd_line, this->jobsList, false);
                this->jobsList->addJob(c, false, pid);
            }
        }
        if(!is_bg) {
            //std::cout << "fg pid: " << pid << endl;
            //foreground command
            cur_fg_pid=pid; //updating pid of current foreground command
            strcpy(cur_fg_cmd, this->cmd_line);

            int stat;
            if (waitpid(pid, &stat, WUNTRACED) < 0) { //wait failed
                perror("smash error: wait failed");
                cur_fg_pid = -1;
                delete CMDLINE;
                return;
            } else {
                if (stat == 0) {//means son terminated normally
                    cur_fg_pid=-1; //son terminated -> there is no command in foreground
                    delete CMDLINE;
                    return;
                }
                /*  else if (stat==1){//means son terminated with errors
                      //handle
                      return;
                  }*/
            }
        }
    }
    else {//fork failed
        perror("smash error: fork failed");
        delete CMDLINE;
        return;
    }
    delete CMDLINE;
    return;
}

void Timeout::execute() {
    this->jobsList->removeFinishedJobs();
    char **args = new char*[20];
    int numArgs=_parseCommandLine(this->cmd_line,args);
    std::string cmd_s;
    for(int i = 2; i <numArgs; ++i){
        cmd_s += args[i];
        cmd_s+= " ";
    }
    char cut_cmd_line[cmd_s.length()];
    strcpy(cut_cmd_line, cmd_s.c_str());
    ExternalCommand* cmd = new ExternalCommand(cut_cmd_line, this->jobsList, true);
    cmd->duration = stoi(args[1]);
    SmallShell& smash = SmallShell::getInstance();
    time_t curr = time(nullptr);
    time_t new_time = stoi(args[1]);
    /*time_out_alarm* timeOutAlarm = new time_out_alarm;
    timeOutAlarm->start = curr;
    timeOutAlarm->duration = new_time;
    timeOutAlarm->total_left = new_time;
    smash.alarms_list.push_back(timeOutAlarm);*/
    time_t min_time = new_time;
    for(int i = 0; i < smash.timeout_list.size(); ++i){
        if(smash.timeout_list[i] != nullptr) {
            double diff = difftime(curr, smash.timeout_list[i]->start);
            smash.timeout_list[i]->total_left = difftime(smash.timeout_list[i]->duration, diff);
            // smash.alarms_list[i]->total_left = smash.alarms_list[i]->duration - (curr - smash.alarms_list[i]->start);
            if (smash.timeout_list[i]->total_left >= 0 && smash.timeout_list[i]->total_left < min_time) {
                min_time = smash.timeout_list[i]->total_left;
            }
        }
    }
    alarm(min_time);
    cmd->execute();
}


SmallShell::SmallShell() {
}

SmallShell::~SmallShell() {
    delete this->jobsList;
    for(int i = 0; i < this->commands.size(); ++i){
        delete commands[i];
    }
    delete cur_fg_cmd;
    for(int i = 0; i < this->timeout_list.size(); ++i){
        if(this->timeout_list.at(i) != nullptr){
            delete this->timeout_list.at(i);
        }
    }

}

/**
* Creates and returns a pointer to Command class which matches the given command line (cmd_line)
*/
Command * SmallShell::CreateCommand(const char* cmd_line) {
  string cmd_s = string(cmd_line);
  if(cmd_s.find(">") != -1){
        return new RedirectionCommand(cmd_line, this->jobsList);
  }
  else if(cmd_s.find("|") != -1){
      return new PipeCommand(cmd_line, this->jobsList);
  }
  else if (cmd_s.find("pwd") == 0) {
    return new GetCurrDirCommand(cmd_line, jobsList);
  }
  else if(cmd_s.find("showpid") == 0){
      return new ShowPidCommand(cmd_line, jobsList);
  }
  else if(cmd_s.find("cd") == 0){
      return new ChangeDirCommand(cmd_line, jobsList);
  }
  else if(cmd_s.find("jobs") == 0){
      return new JobsCommand(cmd_line, jobsList);
  }
  else if(cmd_s.find("chprompt") == 0){
      return new ChpromptCommand(cmd_line, jobsList, &this->name);
  }
  else if(cmd_s.find("kill")==0){
      return new KillCommand(cmd_line,jobsList);
  }
  else if(cmd_s.find("fg")==0){
      return new ForegroundCommand(cmd_line,jobsList);
  }
  else if (cmd_s.find("bg")==0){
      return new BackgroundCommand(cmd_line,jobsList);
  }
  else if(cmd_s.find("quit")==0){
      return new QuitCommand(cmd_line,jobsList);
  }
  else if(cmd_s.find("ls") == 0){
      return new LSCommand(cmd_line, this->jobsList);
  }
  else if(cmd_s.find("timeout") == 0){
      return new Timeout(cmd_line, this->jobsList);
  }
  else if(cmd_s.find("cp") == 0){
      return new CopyCommand(cmd_line, this->jobsList);
  }
  else {
      return new ExternalCommand(cmd_line, this->jobsList, false);
  }
    return nullptr;
}

void SmallShell::executeCommand(const char *cmd_line) {
    Command* cmd = CreateCommand(cmd_line);
    this->commands.push_back(cmd);
    cmd->execute();
}

bool SmallShell::isBuiltIn(char *cmd_line) {
    std::string cmd_s = _trim(cmd_line);
    if (cmd_s.find("pwd") == 0) {
        return true;
    }
    else if(cmd_s.find("showpid") == 0){
        return true;
    }
    else if(cmd_s.find("cd") == 0){
        return true;
    }
    else if(cmd_s.find("jobs") == 0){
        return true;
    }
    else if(cmd_s.find("chprompt") == 0){
        return true;
    }
    else if(cmd_s.find("kill")==0){
        return true;
    }
    else if(cmd_s.find("fg")==0){
        return true;
    }
    else if (cmd_s.find("bg")==0){
        return true;
    }
    else if(cmd_s.find("quit")==0){
        return true;
    }
    else if(cmd_s.find("ls") == 0){
        return true;
    }
    else if(cmd_s.find("timeout") == 0){
        return true;
    }
    else if(cmd_s.find("cp") == 0){
        return true;
    }
    else {
        return false;
    }
}