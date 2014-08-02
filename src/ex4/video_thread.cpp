#include "../common/constant.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#ifdef __cplusplus
}

#endif
#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_events.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <stdlib.h>
#include <assert.h>
#include <time.h>
#include <math.h>

#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define QUEUE_MAX_SIZE 100

#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)

#define FF_ALLOC_EVENT (SDL_USEREVENT)
#define FF_REFRESH_EVENT (SDL_USEREVENT + 1)
#define FF_QUIT_EVENT (SDL_USEREVENT + 2)

#define VIDEO_PICTURE_QUEUE_SIZE 1

FILE *fp=NULL;
typedef struct PacketQueue {
   AVPacketList *first_pkt, *last_pkt;
   int nb_packets;
   int size;
   SDL_mutex *mutex;
   SDL_cond *cond;
} PacketQueue;

typedef struct VideoPicture {
   AVFrame *pFrameYUV;
   int width, height;
   int allocated;
} VideoPicture;

typedef struct VideoState {

   AVFormatContext *pFormatCtx;
   int videoStream, audioStream;
   AVStream *audio_st;
   PacketQueue audioq;
   uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
   unsigned int audio_buf_size;
   unsigned int audio_buf_index;
   AVPacket audio_pkt;
   uint8_t *audio_pkt_data;
   int audio_pkt_size;
   AVStream *video_st;
   PacketQueue videoq;
   uint8_t *buffer;
   VideoPicture pictq[VIDEO_PICTURE_QUEUE_SIZE];
   int pictq_size, pictq_rindex, pictq_windex;
   SDL_mutex *pictq_mutex;
   SDL_cond *pictq_cond;
   SDL_Rect rect;
   SDL_Thread *parse_tid;
   SDL_Thread *video_tid;

   SDL_Window *win;
   SDL_Renderer *renderer;
   SDL_Texture *texture;
   char filename[1024];
   int quit;
   SwsContext *sws_ctx;
   SwrContext *audio_swr;
} VideoState;

/* Since we only have one decoding thread, the Big Struct
   can be global in case we need it. */
VideoState *global_video_state;

void packet_queue_init(PacketQueue *q) {
   memset(q, 0, sizeof(PacketQueue));
   q->mutex = SDL_CreateMutex();
   q->cond = SDL_CreateCond();
}
int packet_queue_put(PacketQueue *q, AVPacket *pkt) {

   AVPacketList *pkt1;
   if (av_dup_packet(pkt) < 0) {
      return -1;
   }
   pkt1 = (AVPacketList *)av_malloc(sizeof(AVPacketList));
   if (!pkt1) return -1;
   pkt1->pkt = *pkt;
   pkt1->next = NULL;

   SDL_LockMutex(q->mutex);

   if (!q->last_pkt)
      q->first_pkt = pkt1;
   else
      q->last_pkt->next = pkt1;
   q->last_pkt = pkt1;
   q->nb_packets++;
   q->size += pkt1->pkt.size;
   SDL_CondSignal(q->cond);

   SDL_UnlockMutex(q->mutex);
   return 0;
}
static int packet_queue_get(PacketQueue *q, AVPacket *pkt, int block) {
   AVPacketList *pkt1;
   int ret;

   SDL_LockMutex(q->mutex);

   for (;;) {

      if (global_video_state->quit) {
         ret = -1;
         break;
      }

      pkt1 = q->first_pkt;
      if (pkt1) {
         q->first_pkt = pkt1->next;
         if (!q->first_pkt) q->last_pkt = NULL;
         q->nb_packets--;
         q->size -= pkt1->pkt.size;
         *pkt = pkt1->pkt;
         av_free(pkt1);
         ret = 1;
         break;
      } else if (!block) {
         ret = 0;
         break;
      } else {
         SDL_CondWait(q->cond, q->mutex);
         
      }
   }
   SDL_UnlockMutex(q->mutex);
   //printf("Lock released!!\n");
   return ret;
}

#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
int audio_decode_frame(VideoState *is, uint8_t *audio_buf, int buf_size) {

   int len1, data_size;
   static AVPacket *pkt = &is->audio_pkt;
   // uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
   AVFrame *decoded_frame = NULL;
   is->audio_pkt_size = 0;
   // printf("start decode,a queue size:%d\n",is->audioq.nb_packets);
   for (;;) {
      while (is->audio_pkt_size > 0) {
         int got_frame = 0;
         if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc())) {
               fprintf(stderr, "Could not allocate audio frame\n");
               exit(1);
            }
         }
         // data_size = buf_size;
         len1 = avcodec_decode_audio4(is->audio_st->codec, decoded_frame,
                                      &got_frame, pkt);
         if (len1 < 0) {
            /* if error, skip frame */
            is->audio_pkt_size = 0;
            break;
         }
         // is->audio_pkt_data += len1;
         // is->audio_pkt_size -= len1;

         if (got_frame) {
            // printf("printf got decode frame\n");
            /* if a frame has been decoded, output it */
            data_size = av_samples_get_buffer_size(NULL, is->audio_st->codec->channels, decoded_frame->nb_samples,is->audio_st->codec->sample_fmt, 1);
            if (data_size < 0) {

               /* This should not occur, checking just for paranoia */
               fprintf(stderr, "Failed to calculate data size\n");
               continue;
               // exit(1);
            }
            // resample needed
            // printf("start resampel");
            AVFrame *temp = av_frame_alloc();
            if (is->audio_st->codec->sample_fmt != AV_SAMPLE_FMT_S16) {
               int nb_samples = decoded_frame->nb_samples;
               int channels = decoded_frame->channels;
               int bytes_per_sample =
                   av_get_bytes_per_sample(is->audio_st->codec->sample_fmt);
               bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);

               int dst_buf_size = nb_samples * bytes_per_sample * channels;
               temp->data[0] = (uint8_t *)av_malloc(dst_buf_size);
               assert(temp->data[0]);
               avcodec_fill_audio_frame(temp, channels, AV_SAMPLE_FMT_S16,
                                        temp->data[0], dst_buf_size, 0);
               if (!is->audio_swr) {
                  uint64_t in_channel_layout = av_get_default_channel_layout(
                      is->audio_st->codec->channels);
                  uint64_t out_channel_layout =
                      av_get_default_channel_layout(channels);
                  is->audio_swr = swr_alloc_set_opts(
                      NULL, out_channel_layout, AV_SAMPLE_FMT_S16,
                      is->audio_st->codec->sample_rate, in_channel_layout,
                      is->audio_st->codec->sample_fmt,
                      is->audio_st->codec->sample_rate, 0, NULL);

                  swr_init(is->audio_swr);
               }
               if (is->audio_swr) {
                  //printf("start resample\n");
                  int ret, out_count;
                  out_count = dst_buf_size / channels /
                              av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                  ret = swr_convert(
                      is->audio_swr, temp->data, out_count,
                      const_cast<const uint8_t **>(decoded_frame->data),
                      nb_samples);
                  if (ret < 0) assert(0);
                  decoded_frame->linesize[0] = temp->linesize[0] =
                      ret * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) *
                      channels;

                  //memcpy(decoded_frame->data[0],
                  // temp->data[0],decoded_frame->linesize[0]);有問題撥音樂檔會記憶體錯誤
                  data_size = ret * channels *
                              av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                  // printf("end resample!!\n");
               }
            } else {
               temp->linesize[0] = decoded_frame->linesize[0];
               memcpy(temp->data[0], decoded_frame->data[0], temp->linesize[0]);
            }
         
            memcpy(audio_buf, temp->data[0], data_size);
            //printf("decode size:%d\n", data_size);
            //fwrite(temp->data[0], 1, data_size, fp);
            av_free(temp);
            av_free(decoded_frame);
            return data_size;
         }
         is->audio_pkt_size -= len1;
         is->audio_pkt_data += len1;

         if (!got_frame) {
            /* stop sending empty packets if the decoder is finished */

            continue;
         }
         if (pkt->size < AUDIO_REFILL_THRESH) {
            printf("packet is too small\n");
         }
      }
      if (pkt->data) av_free_packet(pkt);

      if (is->quit) {

         return -1;
      }
      /* next packet */
      // printf("packet get start audio queue len:%d\n",is->audioq.nb_packets);
      if (packet_queue_get(&is->audioq, pkt, 1) < 0) {
         printf("packet get error!!\n");
         return -1;
      }
      //printf("Get Paket Success!!\n");
      // printf("packet get end audio queue len:%d\n",is->audioq.nb_packets);
      // getchar();
      // is->audio_pkt_data = pkt->data;
      is->audio_pkt_size = pkt->size;
   }
}
void audio_callback(void *userdata, Uint8 *stream, int len) {

   VideoState *is = (VideoState *)userdata;
   int len1, audio_size;
   //if(!fp)
   //{
   //   fp=fopen("test.pcm","wb");
   //}
   timespec tt1, tt2;
   clock_gettime(CLOCK_REALTIME, &tt1);
   while (len > 0) {
      if(is->quit)
      {
         //fclose(fp);
         break;
      }
      if (is->audio_buf_index >= is->audio_buf_size) {
         /* We have already sent all our data; get more */
         audio_size =
             audio_decode_frame(is, is->audio_buf, sizeof(is->audio_buf));
         //printf("audio size:%d\n",audio_size);
         if (audio_size < 0) {
            /* If error, output silence */
            is->audio_buf_size = 1024;
            memset(is->audio_buf, 0, is->audio_buf_size);
         } else {
            is->audio_buf_size = audio_size;
         }
         is->audio_buf_index = 0;
      }
      len1 = is->audio_buf_size - is->audio_buf_index;
      if (len1 > len) len1 = len;
     // fwrite((uint8_t *)is->audio_buf + is->audio_buf_index,1,len1,fp);
      memcpy(stream, (uint8_t *)is->audio_buf + is->audio_buf_index, len1);
      len -= len1;
      stream += len1;
      is->audio_buf_index += len1;
   }
   clock_gettime(CLOCK_REALTIME, &tt2);
    timespec temp;
    if((tt2.tv_nsec-tt1.tv_sec) < 0)
    temp.tv_nsec = 1000000000+tt2.tv_nsec-tt1.tv_nsec;
   printf("audioq size:%d,video size%d\t",is->audioq.nb_packets,is->videoq.nb_packets);
   printf("aduio write time %ld\n",temp.tv_nsec);
}

static Uint32 sdl_refresh_timer_cb(Uint32 interval, void *opaque) {
   SDL_Event event;
   event.type = FF_REFRESH_EVENT;
   event.user.data1 = opaque;
   SDL_PushEvent(&event);
   return 0; /* 0 means stop timer */
}

/* schedule a video refresh in 'delay' ms */
static void schedule_refresh(VideoState *is, int delay) {
   SDL_AddTimer(delay, sdl_refresh_timer_cb, is);
}

void video_display(VideoState *is) {

   SDL_Rect rect;
   VideoPicture *vp;
   float aspect_ratio;
   int w, h, x, y;
   int texture_w, texture_h;
   if (!is->win) {
      is->win = SDL_CreateWindow(
          "Hello World!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
          is->video_st->codec->width, is->video_st->codec->height, 0);
      if (is->win == NULL) {
         std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
         SDL_Quit();
         exit(1);
      }
   }
   if (!is->renderer) {
      is->renderer = SDL_CreateRenderer(is->win, -1, 0);
      if (is->renderer == NULL) {
         printf("Renderer could not be created! SDL Error: %s\n",
                SDL_GetError());
      }
   }
   if (!is->texture) {
      is->texture = SDL_CreateTexture(
          is->renderer, SDL_PIXELFORMAT_YV12, SDL_TEXTUREACCESS_STREAMING,
          is->video_st->codec->width, is->video_st->codec->height);
      if (is->texture == NULL) {
         printf("Texture could not be created! SDL Error: %s\n",
                SDL_GetError());
      }
   }
   is->rect.x = 0;
   is->rect.y = 0;
   is->rect.w = is->video_st->codec->width;
   is->rect.x = is->video_st->codec->height;
   while ((!is->renderer || !is->texture) && !is->quit  ) {
      SDL_Delay(10);
      continue;
   }
   vp = &is->pictq[is->pictq_rindex];
   if (vp->pFrameYUV) {
      /*
      if (is->video_st->codec->sample_aspect_ratio.num == 0) {
         aspect_ratio = 0;
      } else {
         aspect_ratio = av_q2d(is->video_st->codec->sample_aspect_ratio) *
                        is->video_st->codec->width /
                        is->video_st->codec->height;

         if (aspect_ratio <= 0.0) {
            aspect_ratio = (float)is->video_st->codec->width /
                           (float)is->video_st->codec->height;
         }
         SDL_QueryTexture(is->texture, NULL, NULL, &texture_w, &texture_h);
         h = texture_h;
         w = ((int)rint(h * aspect_ratio)) & -3;
         if (w > texture_w) {
            w = texture_w;
            h = ((int)rint(w / aspect_ratio)) & -3;
         }
         x = (texture_w - w) / 2;
         y = (texture_h - h) / 2;
         }
   */
      rect.x = 0;
      rect.y = 0;
      rect.w = is->video_st->codec->width;
      rect.h = is->video_st->codec->height;
      if (is->win && is->renderer && is->texture) {
         //printf("ready to present texture\n");
         int ret = SDL_UpdateYUVTexture(
             is->texture, &rect, vp->pFrameYUV->data[0],
             vp->pFrameYUV->linesize[0], vp->pFrameYUV->data[1],
             vp->pFrameYUV->linesize[1], vp->pFrameYUV->data[2],
             vp->pFrameYUV->linesize[2]);
         if (ret == -1) {
            printf("Update Texture error:%s\n", SDL_GetError());
         }
         ret = SDL_RenderClear(is->renderer);
         //SDL_SetRenderDrawColor(is->renderer, 255, 0, 0, 255);
         SDL_RenderCopy(is->renderer, is->texture, &rect, &rect);
         SDL_RenderPresent(is->renderer);
      
      }
      // SDL_DisplayYUVOverlay(vp->bmp, &rect);
   }
}

void video_refresh_timer(void *userdata) {

   VideoState *is = (VideoState *)userdata;
   VideoPicture *vp;
   if (is->video_st) {
      if (is->pictq_size == 0) {
         schedule_refresh(is, 1);
      } else {
         vp = &is->pictq[is->pictq_rindex];
         /* Now, normally here goes a ton of code
       about timing, etc. we're just going to
       guess at a delay for now. You can
       increase and decrease this value and hard code
       the timing - but I don't suggest that ;)
       We'll learn how to do it for real later.
         */
         schedule_refresh(is, 50);

         /* show the picture! */
         video_display(is);

         /* update queue for next picture! */
         if (++is->pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
            is->pictq_rindex = 0;
         }
         //printf("got lock!!");
         SDL_LockMutex(is->pictq_mutex);
         is->pictq_size--;
         SDL_CondSignal(is->pictq_cond);
         SDL_UnlockMutex(is->pictq_mutex);
         //printf("\t release lock\n");
      }
   } else {
      schedule_refresh(is, 100);
   }
}

/*void alloc_picture(void *userdata) {

  VideoState *is = (VideoState *)userdata;
  VideoPicture *vp;

  vp = &is->pictq[is->pictq_windex];
  if(vp->bmp) {
    // we already have one make another, bigger/smaller
    SDL_FreeYUVOverlay(vp->bmp);
  }
  // Allocate a place to put our YUV image on that screen
  vp->bmp = SDL_CreateYUVOverlay(is->video_st->codec->width,
             is->video_st->codec->height,
             SDL_YV12_OVERLAY,
             screen);
  vp->width = is->video_st->codec->width;
  vp->height = is->video_st->codec->height;

  SDL_LockMutex(is->pictq_mutex);
  vp->allocated = 1;
  SDL_CondSignal(is->pictq_cond);
  SDL_UnlockMutex(is->pictq_mutex);

}*/

int queue_picture(VideoState *is, AVFrame *pFrame) {

   int dst_pix_fmt;
   AVPicture pict;
   VideoPicture *vp;
   /* wait until we have space for a new pic */
   SDL_LockMutex(is->pictq_mutex);
   while (is->pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !is->quit) {
      SDL_CondWait(is->pictq_cond, is->pictq_mutex);
   }
   SDL_UnlockMutex(is->pictq_mutex);

   if (is->quit) return -1;

   // windex is set to 0 initially
   vp = &is->pictq[is->pictq_windex];
   if (!vp->pFrameYUV) {
      vp->pFrameYUV = av_frame_alloc();
   }
   /* allocate or resize the buffer! */
   /*if(!vp->bmp ||
      vp->width != is->video_st->codec->width ||
      vp->height != is->video_st->codec->height) {
     SDL_Event event;

     vp->allocated = 0;
     event.type = FF_ALLOC_EVENT;
     event.user.data1 = is;
     SDL_PushEvent(&event);

     SDL_LockMutex(is->pictq_mutex);
     while(!vp->allocated && !is->quit) {
       SDL_CondWait(is->pictq_cond, is->pictq_mutex);
     }
     SDL_UnlockMutex(is->pictq_mutex);
     if(is->quit) {
       return -1;
     }
   }
   */
   /* We have a place to put our picture on the queue */
   if (vp->pFrameYUV) {
      if (!is->sws_ctx) {

         is->sws_ctx = sws_getContext(
             is->video_st->codec->width, is->video_st->codec->height,
             is->video_st->codec->pix_fmt, is->video_st->codec->width,
             is->video_st->codec->height, PIX_FMT_YUV420P, SWS_BILINEAR, NULL,
             NULL, NULL);
      }
      if (is->sws_ctx) {
         // SDL_LockMutex(is->pictq_mutex);
         dst_pix_fmt = PIX_FMT_YUV420P;

         int numBytes =
             avpicture_get_size(PIX_FMT_YUV420P, is->video_st->codec->width,
                                is->video_st->codec->height);
         if (!is->buffer)
            is->buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));
         avpicture_fill((AVPicture *)vp->pFrameYUV, is->buffer, PIX_FMT_YUV420P,
                        is->video_st->codec->width,
                        is->video_st->codec->height);

         sws_scale(is->sws_ctx, (uint8_t const * const *)pFrame->data,
                   pFrame->linesize, 0, is->video_st->codec->height,
                   vp->pFrameYUV->data, vp->pFrameYUV->linesize);

         // Convert the image into YUV format that SDL uses

         // SDL_UnlockMutex(is->pictq_mutex);
      }
      /* now we inform our display thread that we have a pic ready */
      if (++is->pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
         is->pictq_windex = 0;
      }
      SDL_LockMutex(is->pictq_mutex);
      is->pictq_size++;
      SDL_UnlockMutex(is->pictq_mutex);
   }
   return 0;
}

int video_thread(void *arg) {
   VideoState *is = (VideoState *)arg;
   AVPacket pkt1, *packet = &pkt1;
   int len1, frameFinished;
   AVFrame *pFrame;
   printf("Video Display Thread:%lu\n",(long int)syscall(SYS_gettid));
   pFrame = av_frame_alloc();

   for (;;) {
      if(is->quit)
         break;
      if (packet_queue_get(&is->videoq, packet, 1) < 0) {
         // means we quit getting packets
         printf("video packet get error from queue\n");
         break;
      }
      // Decode video frame
      len1 = avcodec_decode_video2(is->video_st->codec, pFrame, &frameFinished,
                                   packet);

      // Did we get a video frame?
      if (frameFinished) {
         if (queue_picture(is, pFrame) < 0) {
            printf("decode and queuing frame error\n");
            break;
         }
      }
      av_free_packet(packet);
   }
   av_free(pFrame);
   return 0;
}

int stream_component_open(VideoState *is, int stream_index) {

   AVFormatContext *pFormatCtx = is->pFormatCtx;
   AVCodecContext *codecCtx;
   AVCodec *codec;
   SDL_AudioSpec wanted_spec, spec;
   AVDictionary *optionsDict = NULL;
   if (stream_index < 0 || stream_index >= (int)pFormatCtx->nb_streams) {
      return -1;
   }

   // Get a pointer to the codec context for the video stream
   codecCtx = pFormatCtx->streams[stream_index]->codec;

   if (codecCtx->codec_type == AVMEDIA_TYPE_AUDIO) {
      // Set audio settings from codec info
      wanted_spec.freq = codecCtx->sample_rate;
      wanted_spec.format = AUDIO_S16SYS;
      wanted_spec.channels = codecCtx->channels;
      wanted_spec.silence = 0;
      wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
      wanted_spec.callback = audio_callback;
      wanted_spec.userdata = is;

      if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
         fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
         return -1;
      }
   }
   codec = avcodec_find_decoder(codecCtx->codec_id);
   if (!codec || (avcodec_open2(codecCtx, codec, &optionsDict) < 0)) {
      fprintf(stderr, "Unsupported codec!\n");
      return -1;
   }

   switch (codecCtx->codec_type) {
      case AVMEDIA_TYPE_AUDIO:
         is->audioStream = stream_index;
         is->audio_st = pFormatCtx->streams[stream_index];
         is->audio_buf_size = 0;
         is->audio_buf_index = 0;
         memset(&is->audio_pkt, 0, sizeof(is->audio_pkt));
         packet_queue_init(&is->audioq);
         SDL_PauseAudio(0);
         break;
      case AVMEDIA_TYPE_VIDEO:
         is->videoStream = stream_index;
         is->video_st = pFormatCtx->streams[stream_index];

         packet_queue_init(&is->videoq);
         is->video_tid =
             SDL_CreateThread(video_thread, "Video Display Thread", is);
         break;
      default:
         break;
   }
}

int decode_interrupt_cb(void *data) {
   return (global_video_state && global_video_state->quit);
}

int decode_thread(void *arg) {

   VideoState *is = (VideoState *)arg;
   AVFormatContext *pFormatCtx;
   AVPacket pkt1, *packet = &pkt1;

   int video_index = -1;
   int audio_index = -1;
   int i;

   is->videoStream = -1;
   is->audioStream = -1;
   printf("Decode Thread:%lu\n",(long int)syscall(SYS_gettid));
   global_video_state = is;
   // will interrupt blocking functions if we quit!
   // url_set_interrupt_cb(decode_interrupt_cb);
   //新版用Callback

   // Open video file
   if (avformat_open_input(&pFormatCtx, is->filename, NULL, NULL) != 0)
      return -1;  // Couldn't open file

   is->pFormatCtx = pFormatCtx;
   //is->pFormatCtx->interrupt_callback.callback = decode_interrupt_cb;
   //is->pFormatCtx->interrupt_callback.opaque = is;
   // Retrieve stream information
   if (avformat_find_stream_info(pFormatCtx, NULL) < 0)
      return -1;  // Couldn't find stream information

   // Dump information about file onto standard error
   av_dump_format(pFormatCtx, 0, is->filename, 0);

   // Find the first video stream

   for (i = 0; i < (int)pFormatCtx->nb_streams; i++) {
      if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
          video_index < 0) {
         video_index = i;
      }
      if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
          audio_index < 0) {
         audio_index = i;
      }
   }
   if (audio_index >= 0) {
      stream_component_open(is, audio_index);
   }
   if (video_index >= 0) {
      stream_component_open(is, video_index);
   }

   if (is->videoStream < 0 || is->audioStream < 0) {
      fprintf(stderr, "%s: could not open codecs\n", is->filename);
      goto fail;
   }

   // main decode loop

   for (;;) {
      if (is->quit) {
         break;
      }
      // seek stuff goes here
     if (is->audioq.size > MAX_AUDIOQ_SIZE || is->videoq.size >
      MAX_VIDEOQ_SIZE)
      //if(is->audioq.nb_packets >= 1000) 
      //if (is->videoq.nb_packets >= 1000) 
      {
         // printf("A Queue Size:%d V Queue Size:%d\n", is->audioq.size,
         //       is->videoq.size);
         // printf("Video Queue length:%d\n",is->videoq.nb_packets);
         //printf("Audio Queue length:%d\n",is->audioq.nb_packets);
         //printf("Count:%d\n",count);
         //count++;
         //if(count > 2000)
         //{
         //   printf("Dead lock!!\n");
         //   exit(0);
         //}
         //printf("queue fall\n");
         SDL_Delay(10);
         //usleep(10);
         continue;
      }
      if (av_read_frame(is->pFormatCtx, packet) < 0) {

         if (pFormatCtx->pb->error == 0) {
            printf("Read no yet!!\n");
            SDL_Delay(100);  // no error; wait for user input
            continue;
         } else {
            break;
         }
      }
      // Is this a packet from the video stream?
      if (packet->stream_index == is->videoStream) {
         //continue;
         packet_queue_put(&is->videoq, packet);
      } else if (packet->stream_index == is->audioStream) {
         //continue;
         packet_queue_put(&is->audioq, packet);
         //printf("Audio Queue length:%d\n",is->audioq.nb_packets);
      } else {
         av_free_packet(packet);
      }
   }
   /* all done - wait for it */
   while (!is->quit) {
      printf("all done,wait to quit!!\n");
      SDL_Delay(100);
   }

fail:
   if (1) {
      SDL_Event event;
      event.type = FF_QUIT_EVENT;
      event.user.data1 = is;
      SDL_PushEvent(&event);
   }
   return 0;
}

int main(int argc, char *argv[]) {

   SDL_Event event;

   VideoState *is;

   is = (VideoState *)av_mallocz(sizeof(VideoState));

   if (argc < 2) {
      fprintf(stderr, "Usage: test <file>\n");
      exit(1);
   }
   // Register all formats and codecs
   av_register_all();

   if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_TIMER)) {
      fprintf(stderr, "Could not initialize SDL - %s\n", SDL_GetError());
      exit(1);
   }

   /*if (!screen) {
      fprintf(stderr, "SDL: could not set video mode - exiting\n");
      exit(1);
   }*/

   av_strlcpy(is->filename, argv[1], sizeof(is->filename));
   printf("open file:%s\n",is->filename);
   is->pictq_mutex = SDL_CreateMutex();
   is->pictq_cond = SDL_CreateCond();

   schedule_refresh(is, 40);
   is->parse_tid = SDL_CreateThread(decode_thread, "Video Decode Thread", is);
   if (!is->parse_tid) {
      av_free(is);
      return -1;
   }
   for (;;) {

      SDL_WaitEvent(&event);
      switch (event.type) {
         case FF_QUIT_EVENT:
         case SDL_QUIT:
            printf("Ready to Quit!!\n");
            is->quit = 1;
            SDL_LockMutex(is->pictq_mutex);
            SDL_CondBroadcast(is->pictq_cond);
            SDL_UnlockMutex(is->pictq_mutex);
            SDL_LockMutex(is->audioq.mutex);
            SDL_CondBroadcast(is->audioq.cond);
            SDL_UnlockMutex(is->audioq.mutex);
            SDL_LockMutex(is->videoq.mutex);
            SDL_CondBroadcast(is->videoq.cond);
            SDL_UnlockMutex(is->videoq.mutex);
            SDL_Quit();
            return 0;
            break;
         //case FF_ALLOC_EVENT:
            // alloc_picture(event.user.data1);
         //   break;
         case FF_REFRESH_EVENT:
            //printf("refresh screen\n");
            video_refresh_timer(event.user.data1);
            break;
         default:
            break;
      }
   }
   return 0;
}
