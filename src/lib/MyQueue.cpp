#include "../common/common_header.h"
#include "MyQueue.h"
#include "ptr_deleter.h"
MyQueue::MyQueue() {
   quit = 0;
   pthread_mutex_init(&mutex, NULL);
   pthread_cond_init(&cond, NULL);
   size = 0;
}
int MyQueue::force_release_lock() {
   pthread_mutex_lock(&mutex);
   pthread_cond_broadcast(&cond);
   pthread_mutex_unlock(&mutex);
   return 0;
}
int MyQueue::packet_queue_put(
    std::unique_ptr<AVPacket, nodeleter<AVPacket>> &pkt) {
   // printf("queue put!!\n");
   if (av_dup_packet((AVPacket *)pkt.get()) < 0) {
      return -1;
   }
   pthread_mutex_lock(&mutex);
   data.emplace(*pkt);
   // printf("%d\n",data.front().pts);
   // getchar();
   size += pkt->size;
   nb_packets++;
   pthread_cond_signal(&cond);
   pthread_mutex_unlock(&mutex);
   return 0;
}
int MyQueue::packet_queue_get(
    std::unique_ptr<AVPacket, nodeleter<AVPacket>> &pkt, int block) {
   int ret;
   // printf("queue get!!\n");
   pthread_mutex_lock(&mutex);
   for (;;) {
      if (quit) {
         printf("Have to quit!!\n");
         ret = -1;
         break;
      }
      if (!data.empty()) {
         // printf("Get Packet!!\n");
         *pkt = data.front();
         data.pop();
         size -= pkt->size;
         nb_packets--;
         ret = 1;
         break;
      } else if (!block) {
         printf("No Block!!\n");
         ret = -1;
         break;
      } else {
         pthread_cond_wait(&cond, &mutex);
      }
   }
   pthread_mutex_unlock(&mutex);
   return ret;
}
