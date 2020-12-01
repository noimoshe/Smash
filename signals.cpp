#include <iostream>
#include <signal.h>
#include "signals.h"
#include "Commands.h"
#include <sys/wait.h>

using namespace std;

extern pid_t cur_fg_pid;
extern char* cur_fg_cmd;

void ctrlZHandler(int sig_num) {
	if(sig_num==SIGTSTP) {//check if signal is ctrl z
        std::cout<<"smash: got ctrl-Z"<<endl;
        if(cur_fg_pid>0){//there is a process running in foreground
            if(waitpid(cur_fg_pid, NULL, WNOHANG) != 0){
                return;
            }
            int result=kill(cur_fg_pid,SIGSTOP);
            if(result==-1){
                perror("smash error: kill failed");
                return;
            }
            std::cout<<"smash: process "<< cur_fg_pid <<" was stopped"<< endl;
            SmallShell& smash = SmallShell::getInstance();
            ExternalCommand* new_command = new ExternalCommand(cur_fg_cmd, smash.jobsList, false);
            smash.jobsList->addJob(new_command, true, cur_fg_pid); //inserting the stopped job to jobs list
            //updating cur_fg_cmd to -1 because now there is no process in foreground
            cur_fg_pid=-1;
            return;
        }
    }
}

void ctrlCHandler(int sig_num) {
  if(sig_num==SIGINT) { //check if signal is ctrl c
      std::cout << "smash: got ctrl-C" << endl;
      if (cur_fg_pid > 0) {//there is a process running in foreground
          if(waitpid(cur_fg_pid, NULL, WNOHANG) != 0){
              return;
          }
          int result = kill(cur_fg_pid, SIGKILL);
          if (result == -1) {
              perror("smash error: kill failed");
              return;
          }
          std::cout << "smash: process " << cur_fg_pid << " was killed" << endl;
          //updating cur_fg_cmd to -1 because now there is no process in foreground
          cur_fg_pid = -1;
          return;
      }
  }
}

void alarmHandler(int sig_num) {
  if(sig_num==SIGALRM){//check if signal is alarm
      std::cout<<"smash: got an alarm"<<endl;
      SmallShell& smash = SmallShell::getInstance();
      time_t curr = time(nullptr);
      for(int i = 0; i < smash.timeout_list.size(); ++i){
          if(smash.timeout_list.at(i) != nullptr) {
              if(waitpid(smash.timeout_list.at(i)->pid, NULL, WNOHANG) != 0){
                  delete smash.timeout_list.at(i);
                  smash.timeout_list.at(i) = nullptr;
                  i++;
              }
          else if(difftime(curr, smash.timeout_list.at(i)->start) >= smash.timeout_list.at(i)->duration) {
                  int result = kill(smash.timeout_list.at(i)->pid, SIGKILL);
                  if (result == -1) {
                      perror("smash error: kill failed");
                      return;
                  }
                  std::cout << "smash: " << smash.timeout_list.at(i)->cmd_line << " timed out!" << endl;
                  delete smash.timeout_list.at(i);
                  smash.timeout_list.at(i) = nullptr;
              }
          }
      }
      int count = 0;
      for(int i = 0; i < smash.timeout_list.size(); ++i){
          if(smash.timeout_list[i] != nullptr){
              count++;
          }
      }
      if(count == 0){
          return;
      }
      else {
          time_t min_time = INT_MAX;
          for(int i = 0; i < smash.timeout_list.size(); ++i){
              if(smash.timeout_list[i] != nullptr){
                  double diff= difftime(curr,smash.timeout_list[i]->start);
                  smash.timeout_list[i]->total_left=difftime(smash.timeout_list[i]->duration,diff);
                  if(smash.timeout_list[i]->total_left >= 0 && smash.timeout_list[i]->total_left < min_time){
                      min_time = smash.timeout_list[i]->total_left;
                  }
              }
          }
          alarm(min_time);
        }
    }
}


