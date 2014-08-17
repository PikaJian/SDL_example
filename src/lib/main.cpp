
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_events.h>
#include "decoder.h"
#include "../common/common_header.h"
int audio_buf_size = 0;
int audio_buf_index = 0;
void audio_callback(void *userdata, Uint8 *stream, int len) {
   Decoder *d = (Decoder *)userdata;
   int len1, audio_size;
   AVPacket pkt;
   std::unique_ptr<uint8_t, nodeleter<uint8_t>> audio_buf(d->audio_buf,
                                                          nodeleter<uint8_t>());

   std::unique_ptr<AVPacket, nodeleter<AVPacket>> packet(&pkt,
                                                         nodeleter<AVPacket>());
   while (len > 0) {
      if (d->quit) {
         break;
      }
      if (audio_buf_index >= audio_buf_size) {
         /* We have already sent all our data; get more */
         if(d->audioq.packet_queue_get(packet,1) < -1)
            continue;
         audio_size = d->decode_audio_frame(packet, audio_buf);
         // printf("audio size:%d\n",audio_size);
         if (audio_size < 0) {
            /* If error, output silence */
            audio_buf_size = 1024;
            memset(audio_buf.get(), 0, audio_buf_size);
         } else {
            audio_buf_size = audio_size;
         }
         audio_buf_index = 0;
      }
      len1 = audio_buf_size - audio_buf_index;
      if (len1 > len) len1 = len;
      // fwrite((uint8_t *)is->audio_buf + is->audio_buf_index,1,len1,fp);
      memcpy(stream, (uint8_t *)audio_buf.get() + audio_buf_index, len1);
      len -= len1;
      stream += len1;
      audio_buf_index += len1;
   }
}
SDL_Window *win{nullptr};
SDL_Renderer *renderer{nullptr};
SDL_Texture *texture{nullptr};
struct MY_Timer {
   sigevent sigev;
   timer_t timerid;
} timer;

void video_refresh_timer(void *data) {
   struct itimerspec itval, oitval;
   int iDiffInTime = 40;
   itval.it_value.tv_sec = 0;
   itval.it_value.tv_nsec = 0;
   itval.it_interval.tv_sec = 0;
   itval.it_interval.tv_nsec = 0;
   Decoder *d = (Decoder *)data;
   if(!d->pict_Frame)
   {
      d->pict_Frame = std::shared_ptr<AVFrame>(av_frame_alloc(), mydeleter<AVFrame>());

   }
   // printf("Timer Thread:%lu!!!\n", (long int)syscall(SYS_gettid));
   // printf("start handler!!\n");
   // printf(" timer_getoverrun()=%d\n", timer_getoverrun(timerid));
   if (d->GetVideoStream()) {
      if (d->get_picture(d->pict_Frame) < 0) {
         iDiffInTime = 1;
         itval.it_value.tv_nsec = iDiffInTime * 1000000;
         if (timer_settime(timer.timerid, 0, &itval, &oitval) != 0) {
            fprintf(stderr, "time_settime error!");
            return;
         }
      }
      SDL_Rect rect;
      rect.x = 0;
      rect.y = 0;
      rect.w = d->GetVideoStream()->codec->width;
      rect.h = d->GetVideoStream()->codec->height;
      // printf("update texture\n");
      iDiffInTime = 40;
      itval.it_value.tv_nsec = iDiffInTime * 1000000;
      if (timer_settime(timer.timerid, 0, &itval, &oitval) != 0) {
         fprintf(stderr, "time_settime error!");
         return;
      }
      if (win && renderer && texture) {
         int ret = SDL_UpdateYUVTexture(
             texture, &rect, d->pict_Frame->data[0], d->pict_Frame->linesize[0],
             d->pict_Frame->data[1], d->pict_Frame->linesize[1], d->pict_Frame->data[2],
             d->pict_Frame->linesize[2]);
         if (ret == -1) {
            printf("Update Texture error:%s\n", SDL_GetError());
         }
         ret = SDL_RenderClear(renderer);
         // SDL_SetRenderDrawColor(renderer, 255, 0, 0, 255);
         SDL_RenderCopy(renderer, texture, &rect, NULL);
         SDL_RenderPresent(renderer);
      }
   } else {
      iDiffInTime = 100;
      itval.it_value.tv_nsec = iDiffInTime * 1000000;
      if (timer_settime(timer.timerid, 0, &itval, &oitval) != 0) {
         fprintf(stderr, "time_settime error!");
         return;
      }
   }
}

#define TIMER_SIG SIGRTMAX
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
void video_refresh_timer_cb(int sig, siginfo_t *si, void *uc) {
   SDL_Event event;
   event.type = FF_REFRESH_EVENT;
   event.user.data1 = si->si_value.sival_ptr;
   SDL_PushEvent(&event);
}
int main(int argc, char *argv[]) {
   if (argc < 2) {
      fprintf(stderr, "Usage:filename\n");
      exit(-1);
   }
   Decoder d;
   d.open_from_file(argv[1]);
   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
      fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
      exit(1);
   }
   win = SDL_CreateWindow("An SDL2 window",         // window title
                          SDL_WINDOWPOS_UNDEFINED,  // initial x position
                          SDL_WINDOWPOS_UNDEFINED,  // initial y position
                          800,                      // width, in pixels
                          480,                      // height, in pixels
                          SDL_WINDOW_RESIZABLE      // flags - see below
                          );
   if (win == NULL) {
      printf("SDL_CreateWindow Error:%s ", SDL_GetError());
      SDL_Quit();
      exit(1);
   }

   SDL_AudioSpec wanted_spec, spec;
   AVStream *audio_st = d.GetAudioStream();
   wanted_spec.freq = audio_st->codec->sample_rate;
   wanted_spec.format = AUDIO_S16SYS;
   wanted_spec.channels = audio_st->codec->channels;
   wanted_spec.silence = 0;
   wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
   wanted_spec.callback = audio_callback;
   wanted_spec.userdata = &d;

   if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
      fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
      return -1;
   }
   SDL_PauseAudio(0);

   audio_buf_size = spec.size;

   if (!renderer) {
      renderer = SDL_CreateRenderer(win, -1, 1);
      if (renderer == NULL) {
         printf("Renderer could not be created! SDL Error: %s\n",
                SDL_GetError());
      }
   }
   AVStream *video_st = d.GetVideoStream();
   if (!texture) {
      printf("width:%d,height:%d\n", video_st->codec->width,
             video_st->codec->height);
      texture = SDL_CreateTexture(
          renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
          video_st->codec->width, video_st->codec->height);
      if (texture == NULL) {
         printf("Texture could not be created! SDL Error: %s\n",
                SDL_GetError());
      }
   }

   SDL_Rect rect;
   rect.x = 0;
   rect.y = 0;
   rect.w = video_st->codec->width;
   rect.h = video_st->codec->height;

   struct sigaction sa;
   sa.sa_flags = SA_SIGINFO;
   sa.sa_sigaction = video_refresh_timer_cb;
   sigemptyset(&sa.sa_mask);
   if (sigaction(TIMER_SIG, &sa, NULL) == -1) {
      fprintf(stderr, "failed to create timer!!\n");
      exit(-1);
   }

   memset(&timer.sigev, 0, sizeof(timer.sigev));
   timer.sigev.sigev_notify = SIGEV_SIGNAL;
   timer.sigev.sigev_signo = TIMER_SIG;
   timer.sigev.sigev_value.sival_ptr = (void *)&d;

   printf("Main Thread:%lu!!!\n", (long int)syscall(SYS_gettid));
   if (timer_create(CLOCK_REALTIME, &timer.sigev, &timer.timerid) == -1) {
      fprintf(stderr, "failed to create timer!!\n");
      exit(-1);
   }
   struct itimerspec itval, oitval;
   int iDiffInTime = 40;
   itval.it_value.tv_sec = 0;
   itval.it_value.tv_nsec = iDiffInTime * 1000000;
   itval.it_interval.tv_sec = 0;
   itval.it_interval.tv_nsec = 0;
   if (timer_settime(timer.timerid, 0, &itval, &oitval) != 0) {
      fprintf(stderr, "time_settime error!");
      return 1;
   }

   /*std::unique_ptr<AVFrame, mydeleter<AVFrame>> d->pict_Frame_t(av_frame_alloc(),
                                                         mydeleter<AVFrame>());
   for (;;) {
      d.get_picture(d->pict_Frame_t);
      // printf("update texture\n");
      int ret = SDL_UpdateYUVTexture(texture, &rect, d->pict_Frame_t->data[0],
                                     d->pict_Frame_t->linesize[0],
   d->pict_Frame_t->data[1],
                                     d->pict_Frame_t->linesize[1],
   d->pict_Frame_t->data[2],
                                     d->pict_Frame_t->linesize[2]);
      if (ret == -1) {
         printf("Update Texture error:%s\n", SDL_GetError());
      }
      ret = SDL_RenderClear(renderer);
      // SDL_SetRenderDrawColor(is->renderer, 255, 0, 0, 255);
      SDL_RenderCopy(renderer, texture, &rect, NULL);
      SDL_RenderPresent(renderer);
   }*/

   SDL_Event event;
   while (!d.quit) {
      SDL_WaitEvent(&event);
      switch (event.type) {
         case SDL_MOUSEBUTTONDOWN:
            if (event.button.button == SDL_BUTTON_LEFT) printf("key press\n");
            break;
         case SDL_QUIT:
            printf("Have to Quit!!!\n");
            timer_delete(timer.timerid);
            d.quit = 1;
            d.deinit();
            pthread_mutex_lock(&d.pictq_mutex);
            pthread_cond_broadcast(&d.pictq_cond);
            pthread_mutex_unlock(&d.pictq_mutex);
            SDL_Quit();
            break;
         case FF_REFRESH_EVENT:
            video_refresh_timer(event.user.data1);
         default:
            break;
      }
   }

   pthread_join(d.parse_tid, NULL);
   printf("HAVE TO QUIT!!\n");
   printf("d.g_pFrameYUV count:%d\n",d.g_pFrameYUV.use_count());
   //d.pict_Frame->data[0];
   //d.pict_Frame.reset();
   getchar();
   return 0;
}
