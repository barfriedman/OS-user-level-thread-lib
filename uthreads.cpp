//
// Created by barfriedman12 on 4/18/23.
//

#include "Thread.h"
#include "uthreads.h"
#include <iostream>
#include <deque>
#include <vector>
#include <map>

#include <set>
#include <csetjmp>
#include <csignal>
#include <signal.h>
#include <sys/time.h>

static const int RETURN_ERR = -1;
static const int RETURN_SUC = 0;

static const char *const THREAD_LIB_ERR = "thread library error: ";

static const char *const INVALID_QUANTUM = "quantum_usecs must be a positive integer\n";
static const char *const INVALID_ENTRY_POINT = "entry_point can't be null\n";
static const char *const TID_NOT_EXIST = "thread ID doest exist\n";
static const char *const BLOCK_MAIN_ERR = "it's invalid to block the main"
                                          "thread\n ";
static const char *const SLEEP_MAIN_ERR = "it's invalid for the main thread "
                                          "to sleep \n";

static const char *const SYSTEM_ERR = "system error: ";
static const char *const SIG_ACTION_ERR = "sigaction error \n";

#define MASKING sigprocmask(SIG_BLOCK, &mask_set, NULL)
#define UNMASKING sigprocmask(SIG_UNBLOCK, &mask_set, NULL)


/*data structures:
 * all threads with id (will need to keep track of minimal id)
 * READY queue
 * BLOCKED map {tid:thread*}
 * env[max_threads] -- each place in env is a struct containing {__jmpbuf = [...,sp,pc],mask_signal, list of masked }
 * save all threads -- map {tid: thread*} or save only cur running
 *
*/


//function declerations

static const char *const SET_TIMER_ERR = "setitimer error\n";
static const char *const BAD_ALLOC_ERR = "bad alloc error\n";
static const char *const MAX_THREADS = "you reached the max number of threads\n";
static const char *const TIMER_ERR = "setitimer error \n";
void init_timer (int quantum_usecs);
void timer_handler (int sig);
void context_switch ();
void check_sleeping_threads ();
void move_thread_to_running();

std::deque<Thread *> ready_queue;
std::map<int, Thread *> all_threads;
std::map<int, std::set<int>> sleeping;

std::set<int> min_available_tid;

struct sigaction sa = {nullptr};
struct itimerval timer;
sigset_t mask_set;

Thread *running;
int running_tid;
int total_quantum;

void init_timer (int quantum_usecs)
{
  sa.sa_handler = &timer_handler;
  if (sigaction (SIGVTALRM, &sa, NULL) < 0)
  {
    std::cerr << SYSTEM_ERR << SIG_ACTION_ERR;
    exit (1);
  }

  timer.it_value.tv_usec = quantum_usecs;
  timer.it_interval.tv_usec = quantum_usecs;
  if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
  {
    std::cerr << THREAD_LIB_ERR << TIMER_ERR;
  }

}

void init_tid_set ()
{
  for (int i = 0; i < MAX_THREAD_NUM; ++i)
  {
    min_available_tid.insert (i);
  }
}

int uthread_init (int quantum_usecs)
{
  if (quantum_usecs <= 0)
  {
    std::cerr << THREAD_LIB_ERR << INVALID_QUANTUM;
    return RETURN_ERR;
  }
  //init
  sigemptyset (&mask_set);
  sigaddset (&mask_set, SIGALRM);
  total_quantum = 1;
  init_tid_set ();

  //new thread
  Thread *main_trd;
  try
  {
    main_trd = new Thread (0, nullptr, RUNNING);
  }
  catch (const std::bad_alloc &e)
  {
    std::cerr << SYSTEM_ERR << BAD_ALLOC_ERR;
    return RETURN_ERR;
  }
  all_threads.insert (std::make_pair (0, main_trd));
  min_available_tid.erase (0);
  running = main_trd;
  running_tid = 0;
  init_timer (quantum_usecs);
  return RETURN_SUC;
}

int uthread_spawn (thread_entry_point entry_point)
{
  MASKING;
  if (entry_point == nullptr)
  {
    std::cerr << THREAD_LIB_ERR << INVALID_ENTRY_POINT;
    UNMASKING;
    return RETURN_ERR;
  }

  if (!min_available_tid.empty ())
  {
    int min_tid = *min_available_tid.begin ();
    Thread *thread;
    try
    {
      thread = new Thread (min_tid, entry_point, READY);
    }
    catch (const std::bad_alloc &e)
    {
      std::cerr << SYSTEM_ERR << BAD_ALLOC_ERR;
      return RETURN_ERR;
    }
    ready_queue.push_back (thread);
    all_threads.insert (std::make_pair (min_tid, thread));
    min_available_tid.erase (min_tid);
    UNMASKING;
    return min_tid;
  }
  UNMASKING;
  std::cerr << THREAD_LIB_ERR << MAX_THREADS;
  return RETURN_ERR;
}


/***
 * assuming tid exists in the ready_queue, erase it
 * @param tid
 */
void erase_thread_from_ready_by_tid(int tid){
  int index = 0;
  for(int i=0;i<ready_queue.size();i++){
    if(ready_queue.at(i)->getTid() == tid){
      index = i;
      break;
    }
  }
  ready_queue.erase (ready_queue.begin()+index);
}

int uthread_terminate (int tid)
{
  MASKING;
  if (all_threads.find (tid) == all_threads.end ())
  {
    std::cerr << THREAD_LIB_ERR << TID_NOT_EXIST;
    UNMASKING;
    return RETURN_ERR;
  }
  if (tid == 0)
  {
    for (auto pair:all_threads)
    {
      delete pair.second;
      pair.second = nullptr;
    }
    all_threads.clear ();
    UNMASKING;
    //todo delete and release all data structure and thread
    //todo what about env?
    exit (0);
  }
  min_available_tid.insert (tid);
  State cur_state = all_threads.find (tid)->second->getState();
  if(cur_state == READY){
    erase_thread_from_ready_by_tid (tid);
  }
  if(cur_state == SLEEP || cur_state== BS){
    sleeping.find (all_threads.find (tid)->second->get_time_to_wake())
    ->second.erase (tid);
  }
  delete all_threads.find (tid)->second;
  all_threads.find (tid)->second = NULL;
  all_threads.erase (tid);
  if (tid == running_tid){
    running = NULL;
    UNMASKING;
    context_switch ();
  }
  UNMASKING;
  return RETURN_SUC;
}

int uthread_block (int tid)
{
  MASKING;
  if (all_threads.find (tid) == all_threads.end ())
  {
    std::cerr << THREAD_LIB_ERR << TID_NOT_EXIST;
    UNMASKING;
    return RETURN_ERR;
  }
  if (tid == 0)
  {
    std::cerr << THREAD_LIB_ERR << BLOCK_MAIN_ERR;
    UNMASKING;
    return RETURN_ERR;
  }
  if (tid == running_tid)
  {
    //call handler
    running->setState (BLOCKED);
    UNMASKING;
    context_switch ();
      return RETURN_SUC;

  }
  else
  {
    Thread *cur_trd = all_threads.find (tid)->second;
    if (cur_trd->getState () == SLEEP)
    {
      cur_trd->setState (BS);
    }
    else if (cur_trd->getState () == READY)
    {
      cur_trd->setState (BLOCKED);
      erase_thread_from_ready_by_tid (tid);
    }
    UNMASKING;
    return RETURN_SUC;
  }

}

int uthread_resume (int tid)
{
  MASKING;
  if (all_threads.find (tid) == all_threads.end ())
  {
    std::cerr << THREAD_LIB_ERR << TID_NOT_EXIST;
    UNMASKING;
    return RETURN_ERR;
  }
  Thread *curr_thread = all_threads.find (tid)->second;
  if (curr_thread->getState () == BLOCKED)
  {
    //move to ready queue
    curr_thread->setState (READY);
    ready_queue.push_back (curr_thread);
  }
  else if (curr_thread->getState () == BS)
  {
    curr_thread->setState (SLEEP);
  }
  UNMASKING;
  return RETURN_SUC;
}

int uthread_sleep (int num_quantums)
{
  MASKING;
  if (running_tid == 0)
  {
    std::cerr << THREAD_LIB_ERR << SLEEP_MAIN_ERR;
    UNMASKING;
    return RETURN_ERR;
  }
  int release_time = total_quantum + num_quantums; //todo check real
  running->set_time_to_wake (release_time);
  if (sleeping.find (release_time) != sleeping.end ())
  {
    sleeping.find (release_time)->second.insert(running_tid);
  }
  else
  {
    std::set<int> new_set = {running_tid};
    sleeping.insert (std::make_pair (release_time, new_set));
  }
  running->setState (SLEEP);
  UNMASKING;
  context_switch ();

}

void context_switch ()
{
  check_sleeping_threads ();
  total_quantum++; //todo is it here or above?
  if(running==NULL){ // meaning the running thread was terminated
    move_thread_to_running();
    return;
  }
  if (running->getState() == RUNNING){
    running->setState (READY);
    ready_queue.push_back (running);
  }
  int ret_val =  sigsetjmp(*running->getThreadEnv (), 1);
  bool did_just_save_bookmark = ret_val == 0;
  if (did_just_save_bookmark){
    move_thread_to_running();
  }
}


void move_thread_to_running(){
  running = ready_queue.front ();
  ready_queue.pop_front();
  running_tid = running->getTid ();
  running->setState (RUNNING);
  if (setitimer (ITIMER_VIRTUAL, &timer, NULL))
  {
    std::cerr << SYSTEM_ERR << SET_TIMER_ERR;
  }
  siglongjmp (*running->getThreadEnv (), 1);
}
void timer_handler (int sig)
{
  context_switch ();
}

void check_sleeping_threads ()
{
  auto cur_pair = sleeping.find (total_quantum);
  if (cur_pair == sleeping.end ())
  {
    return;
  }
  for (const int &i: cur_pair->second)
  {
    Thread *current_thread = all_threads.find (i)->second;
    if (current_thread->getState () == BS)
    {
      current_thread->setState (BLOCKED);
    }
    else if (current_thread->getState () == SLEEP)
    {
      current_thread->setState (READY);
      ready_queue.push_back (current_thread);
    }
  }
  sleeping.erase (total_quantum);
}

int uthread_get_tid ()
{
  return running_tid;
}

int uthread_get_total_quantums ()
{
  return total_quantum;
}

int uthread_get_quantums (int tid)
{
  MASKING;
  auto pair=  all_threads.find (tid);
  if(pair != all_threads.end() ){
    UNMASKING;
    return pair->second->getQuantumCount ();
  }
  std::cerr<<THREAD_LIB_ERR<<TID_NOT_EXIST;
  UNMASKING;
  return RETURN_ERR;
}