#include "../common/constant.h"
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#ifdef __cplusplus
}
#endif
#include <iostream>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <gtk-3.0/gtk/gtk.h>
#include <gtk-3.0/gdk/gdkx.h>
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define QUEUE_MAX_SIZE 100
SDL_Renderer *renderer;
SDL_Texture *texture;
GtkWidget *sdl_socket;
SDL_Window *sdl_window = NULL;
int quit = 0;
SDL_Rect rect;
SDL_Rect t_rect;
static GtkWidget *gtkwindow = NULL;
void exit() {
   quit = 1;
   printf("fucking quit!!!\n");
   // window->flags |= SDL_WINDOW_HIDDEN;
   uint32_t flags = SDL_GetWindowFlags(sdl_window);
   if (flags & SDL_WINDOW_HIDDEN) printf("Windwo is hidden");
   SDL_QuitSubSystem(SDL_INIT_AUDIO);
   // gtk_main_quit();
   // SDL_ShowWindow(window);
   // SDL_DestroyWindow(window);
   // SDL_Quit();
   // SDL_QuitSubSystem(SDL_INIT_VIDEO);
   // printf("ready to quit!!\n");
   // getchar();
}
static bool full = FALSE;
void clicked(GtkWidget *widget, GdkEventKey *event, gpointer data) {
   if (event->type == GDK_2BUTTON_PRESS) {
      printf("dclicked\n");
      if (!full) {
         gtk_window_fullscreen(GTK_WINDOW(gtkwindow));
         int w, h;
         
      //SDL_DestroyRenderer(renderer);
      //SDL_DestroyTexture(texture);
      //SDL_CreateRenderer(sdl_window,-1,1);
      //texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
      //                            SDL_TEXTUREACCESS_STREAMING,1000,700);
         printf("screen size (%d,%d)\n", w, h);
         full = TRUE;
      } else {

         gtk_window_unfullscreen(GTK_WINDOW(gtkwindow));
         int w, h;
         SDL_GetRendererOutputSize(renderer, &w, &h);
         printf("screen size (%d,%d)\n", w, h);
         full = FALSE;
      }
   }
}
void configure_event(GtkWindow *window, GdkEvent *event, gpointer data) {
   
         SDL_SetWindowSize(sdl_window,event->configure.width,event->configure.height);
   if (renderer) {
      //t_rect.x=0;
      //t_rect.y=0;
      //t_rect.w=event->configure.width;
      //t_rect.w=event->configure.height;
      //int w,h;
      //SDL_GetWindowSize(sdl_window,&w,&h);
      //printf("display window size:(%d,%d)\n",w,h);
      // rect.w = event->configure.width;
      // rect.h = event->configure.height;
      // SDL_SetWindowPosition(sdl_window,50,50);
   }
   //printf("resize!!\n");
}
GtkWidget *create_gtkwindow() {
   GtkWidget *box;
   GtkWidget *statusbar;
   sdl_socket = gtk_drawing_area_new();
   gtkwindow = gtk_window_new(GTK_WINDOW_TOPLEVEL);
   gtk_window_set_position(GTK_WINDOW(gtkwindow), GTK_WIN_POS_CENTER);
   gtk_window_set_default_size(GTK_WINDOW(gtkwindow), 800, 600);
   gtk_window_set_title(GTK_WINDOW(gtkwindow), "Hello World");
   box = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
   gtk_container_add(GTK_CONTAINER(gtkwindow), box);
   //gtk_container_set_border_width(GTK_CONTAINER(gtkwindow),10);
   //gtk_widget_set_size_request(sdl_socket, 800, 600);
   //statusbar = gtk_statusbar_new();
   gtk_box_pack_start(GTK_BOX(box), sdl_socket, TRUE, TRUE, 0);
   //gtk_box_pack_start(GTK_BOX(box), statusbar, FALSE, FALSE, 0);
   g_signal_connect_swapped(G_OBJECT(gtkwindow), "destroy", G_CALLBACK(exit),
                            NULL);
   g_signal_connect(G_OBJECT(gtkwindow), "button-press-event",
                    G_CALLBACK(clicked), NULL);
   g_signal_connect(G_OBJECT(gtkwindow), "configure_event",
                    G_CALLBACK(configure_event), 0);
   gtk_widget_show_all(gtkwindow);
   // gtk_main ();
   return gtkwindow;
}
typedef struct PacketQueue {
   AVPacketList *first_pkt, *last_pkt;
   int nb_packets;
   int size;
   SDL_mutex *mutex;
   SDL_cond *cond;
} PacketQueue;
PacketQueue audioq;
void packet_queue_init(PacketQueue *q) {
   memset(q, 0, sizeof(PacketQueue));
   q->mutex = SDL_CreateMutex();
   q->cond = SDL_CreateCond();
}
FILE *file;
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

      if (quit) {
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
   return ret;
}
#define AUDIO_INBUF_SIZE 20480
#define AUDIO_REFILL_THRESH 4096
static SwrContext *audio_swr = NULL;
static ReSampleContext *resamplerContext = NULL;
int audio_decode_frame(AVCodecContext *aCodecCtx, uint8_t *audio_buf,
                       int buf_size) {

   static AVPacket pkt;
   AVFrame *decoded_frame = NULL;
   static uint8_t *audio_pkt_data = NULL;
   static int audio_pkt_size = 0;
   uint8_t inbuf[AUDIO_INBUF_SIZE + FF_INPUT_BUFFER_PADDING_SIZE];
   int len1, data_size;
   pkt.size = 0;
   for (;;) {
      while (pkt.size > 0) {
         int got_frame = 0;
         if (!decoded_frame) {
            if (!(decoded_frame = av_frame_alloc())) {
               fprintf(stderr, "Could not allocate audio frame\n");
               exit(1);
            }
         }
         len1 =
             avcodec_decode_audio4(aCodecCtx, decoded_frame, &got_frame, &pkt);
         // printf("len1:%d\n", len1);
         if (len1 < 0) {
            /* if error, skip frame */
            printf("frame decode error\n");
            audio_pkt_size = 0;
            break;
         }
         // audio_pkt_data += len1;
         // audio_pkt_size -= len1;
         if (got_frame) {
            /* if a frame has been decoded, output it */
            data_size = av_samples_get_buffer_size(NULL, aCodecCtx->channels,
                                                   decoded_frame->nb_samples,
                                                   aCodecCtx->sample_fmt, 1);
            if (data_size < 0) {
               /* This should not occur, checking just for paranoia */
               fprintf(stderr, "Failed to calculate data size\n");
               continue;
            }
            // resample needed
            AVFrame *temp = av_frame_alloc();
            if (aCodecCtx->sample_fmt != AV_SAMPLE_FMT_S16) {
               int nb_samples = decoded_frame->nb_samples;
               int channels = decoded_frame->channels;
               int bytes_per_sample =
                   av_get_bytes_per_sample(aCodecCtx->sample_fmt);
               bytes_per_sample = av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
               int dst_buf_size = nb_samples * bytes_per_sample * channels;
               temp->data[0] = (uint8_t *)av_malloc(dst_buf_size);
               assert(temp->data[0]);
               avcodec_fill_audio_frame(temp, channels, AV_SAMPLE_FMT_S16,
                                        temp->data[0], dst_buf_size, 0);
               if (!audio_swr) {
                  uint64_t in_channel_layout =
                      av_get_default_channel_layout(aCodecCtx->channels);
                  uint64_t out_channel_layout =
                      av_get_default_channel_layout(channels);
                  audio_swr = swr_alloc_set_opts(
                      NULL, out_channel_layout, AV_SAMPLE_FMT_S16,
                      aCodecCtx->sample_rate, in_channel_layout,
                      aCodecCtx->sample_fmt, aCodecCtx->sample_rate, 0, NULL);
                  swr_init(audio_swr);
               }
               if (audio_swr) {
                  // printf("start resample\n");
                  int ret, out_count;
                  out_count = dst_buf_size / channels /
                              av_get_bytes_per_sample(AV_SAMPLE_FMT_S16);
                  ret = swr_convert(
                      audio_swr, temp->data, out_count,
                      const_cast<const uint8_t **>(decoded_frame->data),
                      nb_samples);
                  if (ret < 0) assert(0);
                  decoded_frame->linesize[0] = temp->linesize[0] =
                      ret * av_get_bytes_per_sample(AV_SAMPLE_FMT_S16) *
                      channels;
                  // memcpy(decoded_frame->data[0],
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
            // printf("decode size:%d\n", data_size);
            // fwrite(temp->data[0], 1, data_size, file);
            av_free(temp);
            av_free(decoded_frame);
            return data_size;
         }
         // pkt.size -= len1;
         pkt.size -= len1;
         pkt.data += len1;
         if (!got_frame) {
            /* stop sending empty packets if the decoder is finished */

            continue;
         }
         if (pkt.size < AUDIO_REFILL_THRESH) {
            printf("packet is too small\n");
         }
      }
      /* We have data, return it and come back for more later */
      if (pkt.data) av_free_packet(&pkt);

      if (quit) {
         return -1;
      }

      if (packet_queue_get(&audioq, &pkt, 1) < 0) {
         return -1;
      }
      // audio_pkt_data = pkt.data;
      audio_pkt_size = pkt.size;
   }
}
void audio_callback(void *userdata, Uint8 *stream, int len) {

   AVCodecContext *aCodecCtx = (AVCodecContext *)userdata;
   int len1, audio_size;
   static uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2];
   static unsigned int audio_buf_size = 0;
   static unsigned int audio_buf_index = 0;
   // printf("start callback\n");
   while (len > 0) {
      // printf("start play sound\n");
      if (audio_buf_index >= audio_buf_size) {
         /* We have already sent all our data; get more */
         audio_size =
             audio_decode_frame(aCodecCtx, audio_buf, sizeof(audio_buf));
         // fwrite(audio_buf, 1, audio_size, file);
         if (audio_size < 0) {
            /* If error, output silence */
            audio_buf_size = 1024;  // arbitrary?
            memset(audio_buf, 0, audio_buf_size);
         } else {
            audio_buf_size = audio_size;
         }
         audio_buf_index = 0;
      }
      len1 = audio_buf_size - audio_buf_index;
      if (len1 > len) len1 = len;
      // SDL_MixAudio(stream,&(audio_buf[audio_buf_index]), len1,
      // SDL_MIX_MAXVOLUME);
      memcpy(stream, (uint8_t *)audio_buf + audio_buf_index, len1);
      len -= len1;
      stream += len1;
      // audio_buf += len1;
      audio_buf_index += len1;
   }
}
int main(int argc, char **argv) {

   AVFormatContext *pFormatCtx = NULL;
   int videoStream, audioStream;
   AVCodecContext *pCodecCtx;
   AVCodecContext *aCodecCtx;
   AVCodec *pCodec;
   AVCodec *aCodec;
   AVFrame *pFrame;
   AVFrame *pFrameYUV;
   AVFrame *pFrameRGB;
   AVPacket packet;
   AVDictionary *optionsDict = NULL;
   SwsContext *sws_ctx;
   SDL_Event event;
   SDL_AudioSpec wanted_spec, spec;
   SDL_Surface *screensurface = NULL;
   SDL_Surface *pictsurface = NULL;
   if (argc < 2) {
      fprintf(stderr, "Usage: test <file>\n");
      exit(1);
   }

   gtk_init(&argc, &argv);
   av_register_all();
   if (avformat_open_input(&pFormatCtx, argv[1], NULL, NULL) != 0) {
      return -1;  // Couldn't open file
   }
   // Retrieve stream information
   if (avformat_find_stream_info(pFormatCtx, NULL) < 0) {
      return -1;  // Couldn't find stream information
   }

   // Dump information about file onto standard error
   av_dump_format(pFormatCtx, 0, argv[1], 0);
   // Find the first video stream
   videoStream = -1;
   audioStream = -1;
   for (int i = 0; i < pFormatCtx->nb_streams; i++) {
      if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
          videoStream < 0) {
         videoStream = i;
      }
      if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
          audioStream < 0) {
         audioStream = i;
      }
   }
   if (videoStream == -1 && audioStream == -1) {
      printf("No Stream Read!!\n");
      return -1;
   }

   //////////////////////////////////////////////////////
   // SDL

   quit = 0;
   /*if (videoStream == -1) {
      printf("Video Read Stream Error!!\n");
      return -1;  // Didn't find a video stream
   }
   if (audioStream == -1) {
      printf("Audio Read Stream Error!!\n");
      return -1;  // Didn't find a audio stream
   }*/
   if (videoStream != -1) {
      pCodecCtx = pFormatCtx->streams[videoStream]->codec;
      pCodec = avcodec_find_decoder(pCodecCtx->codec_id);
      if (avcodec_open2(pCodecCtx, pCodec, &optionsDict) < 0) {
         printf("Video Codec Error!!\n");
         return -1;  // Could not open video codec
      }
      pFrame = av_frame_alloc();
      pFrameYUV = av_frame_alloc();
      if (pFrameYUV == NULL) {
         return -1;
      }
      printf("Width:%d Hieght:%d\n", pCodecCtx->width, pCodecCtx->height);
      sws_ctx = sws_getContext(pCodecCtx->width, pCodecCtx->height,
                               pCodecCtx->pix_fmt, pCodecCtx->width,
                               pCodecCtx->height, PIX_FMT_YUV420P, SWS_BICUBIC,
                               NULL, NULL, NULL);
      int numBytes = avpicture_get_size(PIX_FMT_YUV420P, pCodecCtx->width,
                                        pCodecCtx->height);
      uint8_t *buffer = (uint8_t *)av_malloc(numBytes * sizeof(uint8_t));

      avpicture_fill((AVPicture *)pFrameYUV, buffer, PIX_FMT_YUV420P,
                     pCodecCtx->width, pCodecCtx->height);

      // Read frames and save first five frames to disk
      rect.x = 0;
      rect.y = 0;
      rect.w = pCodecCtx->width;
      rect.h = pCodecCtx->height;
      memcpy(&t_rect,&rect,sizeof(SDL_Rect));
      if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
         std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
         return 1;
      }
      // SDL_EventState(SDL_WINDOWEVENT, SDL_IGNORE);
      // SDL_EventState(SDL_SYSWMEVENT, SDL_IGNORE);
      create_gtkwindow();
      sdl_window = SDL_CreateWindowFrom(
          (void *)GDK_WINDOW_XID(gtk_widget_get_window(sdl_socket)));
      // SDL_Window *win = SDL_CreateWindow(
      //    "Hello World!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      //    pCodecCtx->width, pCodecCtx->height, 0);

      if (sdl_window == NULL) {
         fprintf(stderr, "Could not create window: %s\n", SDL_GetError());
         SDL_Quit();
         return 1;
      }

      // screensurface = SDL_GetWindowSurface(window);
      // if (!screensurface) {
      //   printf("Create Renderer failed!!\n");
      //   exit(0);
      //}
      // if (win == NULL) {
      //   std::cout << "SDL_CreateWindow Error: " << SDL_GetError() <<
      // std::endl;
      //   SDL_Quit();
      //   return 1;
      //}
      renderer = SDL_CreateRenderer(sdl_window, -1, 1);
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                                  SDL_TEXTUREACCESS_STREAMING, 800, 600);
   }

   if (audioStream != -1) {
      printf("Audio init\n");
      aCodecCtx = pFormatCtx->streams[audioStream]->codec;
      aCodec = avcodec_find_decoder(aCodecCtx->codec_id);

      wanted_spec.freq = aCodecCtx->sample_rate;
      wanted_spec.format = AUDIO_S16SYS;
      wanted_spec.channels = aCodecCtx->channels;
      wanted_spec.silence = 0;
      wanted_spec.samples = SDL_AUDIO_BUFFER_SIZE;
      wanted_spec.callback = audio_callback;
      wanted_spec.userdata = aCodecCtx;

      if (SDL_OpenAudio(&wanted_spec, &spec) < 0) {
         fprintf(stderr, "SDL_OpenAudio: %s\n", SDL_GetError());
         return -1;
      }

      if (avcodec_open2(aCodecCtx, aCodec, &optionsDict) < 0) {
         printf("Video Codec Error!!\n");
         return -1;  // Could not open audio codec
      }
      packet_queue_init(&audioq);
      SDL_PauseAudio(0);
   }
   // SDL_PixelFormat *format = screensurface->format;

   // pictsurface = SDL_CreateRGBSurface(
   //    0, pCodecCtx->width, pCodecCtx->height, format->BitsPerPixel,
   //    format->Rmask, format->Gmask, format->Bmask, format->Amask);
   // if (!pictsurface) {
   //   printf("pictsurface create failed\n");
   //   return -1;
   //}
   // SDL_SetWindowFullscreen(window, SDL_WINDOW_FULLSCREEN_DESKTOP);
   while (!quit) {
      while (av_read_frame(pFormatCtx, &packet) >= 0 && !quit) {
         // Is this a packet from the video stream?
         while (gtk_events_pending()) {
            gtk_main_iteration_do(FALSE);
         }
         if (packet.stream_index == videoStream) {
            // Decode video frame
            int frameFinished;
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if (frameFinished) {
               sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                         pFrame->linesize, 0, pCodecCtx->height,
                         pFrameYUV->data, pFrameYUV->linesize);
               // printf(
               //    "surface width:%d hieght:%d pitchs:%d byte per pixel:%d\n",
               //    screensurface->w, screensurface->h, screensurface->pitch,
               //    screensurface->format->BytesPerPixel);
               // SDL_FillRect(pictsurface,
               // NULL,SDL_MapRGB(screensurface->format,255,255,255));
               /*SDL_LockSurface(pictsurface);
               for (int row = 0; row < pictsurface->h; row++) {

                  for (int col = 0; col < pictsurface->w; col++) {
                     int p = col * 3 + row * pFrameRGB->linesize[0];
                     //((uint32_t*)pictsurface->pixels)[col] =
                     //  (pFrameRGB->data[0][p]<<16) |
                     // (pFrameRGB->data[0][p+1]<<8)
                     //  | pFrameRGB->data[0][p+2];
                     int bpp = pictsurface->format->BytesPerPixel;
                     uint8_t *pix = (uint8_t *)pictsurface->pixels +
                                    row * pictsurface->pitch + col * bpp;
                     *(uint32_t *)pix = SDL_MapRGB(
                         pictsurface->format, pFrameRGB->data[0][p + 2],
                         pFrameRGB->data[0][p + 1], pFrameRGB->data[0][p]);
                     //((uint8_t **)pictsurface->pixels)[col][0] = 255 << 16;
                     //((uint8_t **)pictsurface->pixels)[col][1] = 255 << 8;
                     //((uint8_t **)pictsurface->pixels)[col][2] = 255;
                  }
                  // pictsurface->pixels += pictsurface->pitch;
               }*/
               // SDL_UnlockSurface(pictsurface);
               // YV12 Texture專用 正常顏色for YUV420
               SDL_UpdateYUVTexture(texture, &rect, pFrameYUV->data[0],
                                    pFrameYUV->linesize[0], pFrameYUV->data[1],
                                    pFrameYUV->linesize[1], pFrameYUV->data[2],
                                    pFrameYUV->linesize[2]);
               // SDL_UpdateTexture(texture, &rect,
               // pFrameYUV->data[0],pFrameYUV->linesize[0]);//這個顏色怪怪的
               SDL_RenderClear(renderer);
               //SDL_SetRenderDrawColor(renderer,255,255,255,0);
               SDL_RenderCopy(renderer, texture, &rect,NULL);
               SDL_RenderPresent(renderer);
               // SDL_BlitSurface(pictsurface, NULL, screensurface, NULL);
               // SDL_UpdateWindowSurface(window);
            }
            // SDL_Delay(50);
         } else if (packet.stream_index == audioStream) {
            // printf("get audio packet\n");
            while (audioq.nb_packets > QUEUE_MAX_SIZE) SDL_Delay(50);
            packet_queue_put(&audioq, &packet);
         } else {
            // Free the packet that was allocated by av_read_frame
            // av_free_packet(&packet);
         }

         SDL_PollEvent(&event);
         switch (event.type) {
            case SDL_KEYDOWN:
               printf("you enter key\n");
               break;
            case SDL_QUIT:
               printf("Have to Quit!!!\n");
               quit = 1;
               SDL_Quit();
               // exit(0);
               break;
            default:
               break;
         }
      }
   }

   if (videoStream != -1) {
      printf("destory video codec resource\n");
      SDL_DestroyTexture(texture);

      sws_freeContext(sws_ctx);
      // Free the YUV frame
      av_free(pFrame);
      av_free(pFrameYUV);

      avcodec_close(pCodecCtx);
   }
   if (audioStream != -1) {
      // Close the codec
      avcodec_close(aCodecCtx);
   }
   // Close the video file
   avformat_close_input(&pFormatCtx);

   return 0;
}
