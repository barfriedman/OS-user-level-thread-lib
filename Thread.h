//
// Created by barfriedman12 on 4/18/23.
//


#ifndef EX2_THREAD_H
#define EX2_THREAD_H

#include <csetjmp>
#include "uthreads.h"

enum State{READY, RUNNING, BLOCKED, SLEEP, BS};

class Thread {
 private:

  const int tid;
  const thread_entry_point entryPoint;
  State state;
  char *stack;
  sigjmp_buf thread_env;
  int quantum_counter;
  int time_to_wake;
 public:
  void set_time_to_wake (int time_to_wake);
 public:
  int get_time_to_wake () const;

 public:

  Thread(int tid, thread_entry_point entry_point, State state);
  void setState(State state);

  int getTid() const;

  State getState() const;
  int getQuantumCount() const;

  sigjmp_buf* getThreadEnv();
  void updateQuantumCounter();

  virtual ~Thread();
};


#endif //EX2_THREAD_H
