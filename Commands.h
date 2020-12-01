#ifndef SMASH_COMMAND_H_
#define SMASH_COMMAND_H_

#include <vector>
#include <string>
#include <climits>

#define COMMAND_ARGS_MAX_LENGTH (200)
#define COMMAND_MAX_ARGS (20)
#define HISTORY_MAX_RECORDS (50)
#include <unistd.h>
#include <time.h>


class JobsList;

struct time_out {
    pid_t pid;
    time_t start;
    time_t duration;
    time_t total_left;
    std::string cmd_line;
};

class Command {
 public:
    char* cmd_line;
    JobsList* jobsList;
    Command(const char* cmd_line, JobsList* jobsList);
    virtual ~Command();
    virtual void execute() = 0;
    //virtual void prepare();
    //virtual void cleanup();
};

class BuiltInCommand : public Command {
 public:
    BuiltInCommand(const char* cmd_line, JobsList* jobsList) : Command(cmd_line, jobsList) {};
    virtual ~BuiltInCommand() {}
};

class ExternalCommand : public Command {
 public:
  ExternalCommand(const char* cmd_line, JobsList* jobsList, bool is_timeout) : Command(cmd_line, jobsList), is_timeout(is_timeout) {};
  virtual ~ExternalCommand() {}
  void execute() override;
  bool is_timeout;
  time_t duration = 0;
};

class PipeCommand : public Command {
 public:
  PipeCommand(const char* cmd_line, JobsList* jobs) : Command(cmd_line, jobs) {};
  virtual ~PipeCommand() {}
  void execute() override;
};

class RedirectionCommand : public Command {
 public:
  explicit RedirectionCommand(const char* cmd_line, JobsList* jobsList) : Command(cmd_line, jobsList) {};
  virtual ~RedirectionCommand() {}
  void execute() override;
  //void prepare() override;
  //void cleanup() override;
};

class ChangeDirCommand : public BuiltInCommand {
public:
    ChangeDirCommand(const char *cmdLine, JobsList* jobsList) : BuiltInCommand(cmdLine, jobsList) {};
    virtual ~ChangeDirCommand();
    void execute() override;
};

class GetCurrDirCommand : public BuiltInCommand {
 public:
  GetCurrDirCommand(const char* cmd_line, JobsList* jobsList) : BuiltInCommand(cmd_line, jobsList) {};
  virtual ~GetCurrDirCommand() {}
  void execute() override;
};

class ShowPidCommand : public BuiltInCommand {
 public:
  ShowPidCommand(const char* cmd_line, JobsList* jobsList) : BuiltInCommand(cmd_line, jobsList) {};
  virtual ~ShowPidCommand() {}
  void execute() override;
};

class JobsList;
class QuitCommand : public BuiltInCommand {
public:
    QuitCommand(const char* cmd_line, JobsList* jobs): BuiltInCommand(cmd_line, jobs){};
    virtual ~QuitCommand() {}
    void execute() override;
};

class JobsList {
 public:
  class JobEntry {
   public:
      int jobID;
      Command* cmd;
      bool isStopped;
      time_t start;
      pid_t pid;
      JobEntry(int jobID,Command* cmd, bool isStopped = false, pid_t pid = 0);
      ~JobEntry() {
          delete this->cmd;
      };
  };
    std::vector<JobEntry*>* jobs;
    int maxID;
    int jobsCount;

 public:
  JobsList();
  ~JobsList();
  void addJob(Command* cmd, bool isStopped = false, pid_t pid = 0);
  void printJobsList();
  /*void killAllJobs();*/
  void removeFinishedJobs();
  JobEntry * getJobById(int jobId);
  void removeJobById(int jobId);
  JobEntry * getLastJob(int* lastJobId);
  JobEntry *getLastStoppedJob(int *jobId);
  int getsize(){ return (this->maxID -1); }
  void changePid(int jobId, pid_t pid) { this->jobs->at(jobId)->pid = pid; }
  void updateMaxId();
  void updateJobsCount();
};

class JobsCommand : public BuiltInCommand {
 public:
  JobsCommand(const char* cmd_line, JobsList* jobsList) : BuiltInCommand(cmd_line, jobsList) {};
  virtual ~JobsCommand() {}
  void execute() override;
};

class KillCommand : public BuiltInCommand {
 public:
  KillCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs) {};
  virtual ~KillCommand() {}
  void execute() override;
};


class ForegroundCommand : public BuiltInCommand {
 public:
  ForegroundCommand(const char* cmd_line, JobsList* jobs): BuiltInCommand(cmd_line, jobs) {};
  virtual ~ForegroundCommand() {}
  void execute() override;
};

class BackgroundCommand : public BuiltInCommand {
 public:
  BackgroundCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs) {};
  virtual ~BackgroundCommand() {}
  void execute() override;
};


class LSCommand : public BuiltInCommand {
public:
    LSCommand(const char* cmd_line, JobsList* jobs) : BuiltInCommand(cmd_line, jobs) {};
    virtual ~LSCommand() {}
    void execute() override;
};

class ChpromptCommand : public BuiltInCommand { // TODO: Add your data members
public:
    std::string* name;
    ChpromptCommand(const char* cmd_line, JobsList* jobs, std::string* name): BuiltInCommand(cmd_line, jobs) {
                    this->name=name;
    };
    virtual ~ChpromptCommand() {}
    void execute() override;
};

class Timeout : public Command {
public:
    Timeout(const char* cmd_line, JobsList* jobs) : Command(cmd_line, jobs) {};
    virtual ~Timeout() {};
    void execute() override;
};

class CopyCommand : public Command {
public:
    CopyCommand(const char* cmd_line, JobsList* jobs) : Command(cmd_line, jobs) {};
    virtual ~CopyCommand() {};
    void execute() override;
};


class SmallShell {
 private:
  SmallShell();
 public:
    pid_t pid = getpid();
    std::string name = "smash";
    JobsList* jobsList = new JobsList();
    std::vector<Command*> commands;
    std::vector<time_out*> timeout_list;
    Command *CreateCommand(const char* cmd_line);
  SmallShell(SmallShell const&)      = delete; // disable copy ctor
  void operator=(SmallShell const&)  = delete; // disable = operator
  static SmallShell& getInstance() // make SmallShell singleton
  {
    static SmallShell instance; // Guaranteed to be destroyed.
    // Instantiated on first use.
    return instance;
  }
  ~SmallShell();
  void executeCommand(const char* cmd_line);
  bool isBuiltIn(char* cmd_line);
};

#endif //SMASH_COMMAND_H_
