// Long-term locks for processes
#ifndef SLEEPLOCK_H
#define SLEEPLOCK_H
#include "spinlock.h"     //lab10
struct sleeplock {
  uint locked;       // Is the lock held?
  struct spinlock lk; // spinlock protecting this sleep lock
  
  // For debugging:
  char *name;        // Name of lock.
  int pid;           // Process holding lock
};


#endif
