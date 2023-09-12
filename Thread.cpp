//
// Created by barfriedman12 on 4/18/23.
//

#include <csignal>
#include "Thread.h"

#ifdef __x86_64__
/* code for 64 bit Intel arch */

typedef unsigned long address_t;
#define JB_SP 6
#define JB_PC 7

/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
  address_t ret;
  asm volatile("xor    %%fs:0x30,%0\n"
               "rol    $0x11,%0\n"
  : "=g" (ret)
  : "0" (addr));
  return ret;
}
//todo check if needed
#else
/* code for 32 bit Intel arch */

typedef unsigned int address_t;
#define JB_SP 4
#define JB_PC 5


/* A translation is required when using an address of a variable.
   Use this as a black box in your code. */
address_t translate_address(address_t addr)
{
    address_t ret;
    asm volatile("xor    %%gs:0x18,%0\n"
                 "rol    $0x9,%0\n"
    : "=g" (ret)
    : "0" (addr));
    return ret;
}

#endif

Thread::Thread(int tid, thread_entry_point entry_point , State state) : tid(tid), entryPoint(entry_point), state(state)  {
  this->time_to_wake = 0;
  if(tid !=0){
    this->stack = new char[STACK_SIZE]; //todo dont forget to release
    address_t sp = (address_t) stack + STACK_SIZE - sizeof(address_t);
    address_t pc = (address_t) entryPoint;
    sigsetjmp(thread_env, 1);
    (thread_env->__jmpbuf)[JB_SP] = translate_address(sp);
    (thread_env->__jmpbuf)[JB_PC] = translate_address(pc);
    sigemptyset(&thread_env->__saved_mask);
    quantum_counter = 0;
  }
  else{
    quantum_counter = 1;
  }


}

void Thread::setState(State state) {
  Thread::state = state;
  // update quantum counter
  if (state == RUNNING){
    updateQuantumCounter();
  }
}

int Thread::getTid() const {
  return tid;
}

State Thread::getState() const {
  return state;
}


int Thread::getQuantumCount() const{
  return quantum_counter;
}




Thread::~Thread() {
  if(this->tid !=0){
    delete[] stack;
  }

}

sigjmp_buf* Thread::getThreadEnv()  {
  return &thread_env;
}

void Thread::updateQuantumCounter() {
  quantum_counter++;
}
int Thread::get_time_to_wake () const
{
  return time_to_wake;
}
void Thread::set_time_to_wake (int time_to_wake)
{
  Thread::time_to_wake = time_to_wake;
}



