#pragma once

#include <string>
#include <vector>
#include <functional>
#include <mutex>

struct PendingCommand;

class Task {
public:
  virtual ~Task() = default;
  enum State {
    Running,
    Done
  };
  State state = Running;
  int errorcode = 0;
  std::vector<char> outbuffer;
};

class Executor {
public:
  Executor();
  ~Executor();
  void Run(PendingCommand* cmd);
  void Start();
  bool Busy();
private:
  void RunMoreCommands();
  std::mutex m;
  std::vector<PendingCommand*> commands;
  std::vector<Task*> activeTasks;
};


