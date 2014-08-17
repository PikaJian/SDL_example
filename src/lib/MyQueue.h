#ifndef MYQUEUE_H
#define MYQUEUE_H
#include "../common/common_header.h"
#include "ptr_deleter.h"
#include <queue>
#include <pthread.h>
class MyQueue {
  public:
   int nb_packets;
   int size;
   int quit;
   MyQueue();
   int packet_queue_put(std::unique_ptr<AVPacket, nodeleter<AVPacket>> &);
   int packet_queue_get(std::unique_ptr<AVPacket, nodeleter<AVPacket>> &, int);
   int force_release_lock();

  private:
   pthread_mutex_t mutex;
   pthread_cond_t cond;
   std::queue<AVPacket> data;
};

#endif
