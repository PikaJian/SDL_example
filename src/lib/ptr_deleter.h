#ifndef PTR_DELETER
#define PTR_DELETER
#include "../common/common_header.h"
template <typename T>
struct nodeleter {
   nodeleter() {}
   nodeleter(const nodeleter &) {}
   nodeleter(nodeleter &) {}
   nodeleter(nodeleter &&) {}
   void operator()(T *data) const {}
};
template <typename T>
struct mydeleter {
  public:
   mydeleter() {}
   mydeleter(const mydeleter &) {
      // printf("mydeleter copy ctor\n");
   }
   mydeleter(mydeleter &) {
      // printf("mydeleter non-const copy ctor\n");
   }
   mydeleter(mydeleter &&) {
      // printf("mydeleter move ctor \n");
   }
   void operator()(T *data) {
      printf("free data\n!!");
      av_free(data);
   }
   mydeleter &operator=(mydeleter other) {
      printf("mydeleter move assignment!!\n");
      return *this;
   }
};
template <>
struct mydeleter<AVPacket> {
   mydeleter() {}
   mydeleter(const mydeleter &) { printf("mydeleter copy ctor\n"); }
   mydeleter(mydeleter &) { printf("mydeleter non-const copy ctor\n"); }
   mydeleter(mydeleter &&) { printf("mydeleter move ctor \n"); }
   void operator()(AVPacket *data) const { av_free_packet(data); }
};
template <>
struct mydeleter<AVFrame> {
   mydeleter() {}
   mydeleter(const mydeleter &) {
      // printf("mydeleter copy ctor\n");
   }
   mydeleter(mydeleter &) {
      // printf("mydeleter non-const copy ctor\n");
   }
   mydeleter(mydeleter &&) {
      // printf("mydeleter move ctor \n");
   }
   void operator()(AVFrame *data) const {
      printf("free Frame\n!!");
      av_frame_free(&data);
   }
   mydeleter &operator=(mydeleter other) {
      printf("mydeleter move assignment!!\n");
      return *this;
   }
};
#endif
