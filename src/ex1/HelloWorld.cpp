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
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define QUEUE_MAX_SIZE 100
typedef struct PacketQueue {
   AVPacketList *first_pkt, *last_pkt;
   int nb_packets;
   int size;
   SDL_mutex *mutex;
   SDL_cond *cond;
} PacketQueue;
PacketQueue audioq;
int quit = 0;
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
         fclose(file);
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
         fclose(file);
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

   file = fopen("test1.pcm", "wb");
   AVFormatContext *pFormatCtx = NULL;
   int videoStream, audioStream;
   AVCodecContext *pCodecCtx;
   AVCodecContext *aCodecCtx;
   AVCodec *pCodec;
   AVCodec *aCodec;
   AVFrame *pFrame;
   AVFrame *pFrameYUV;
   AVPacket packet;
   AVDictionary *optionsDict = NULL;
   SwsContext *sws_ctx;
   SDL_Renderer *renderer;
   SDL_Texture *texture;
   SDL_Rect rect;
   SDL_Event event;
   SDL_AudioSpec wanted_spec, spec;
   if (argc < 2) {
      fprintf(stderr, "Usage: test <file>\n");
      exit(1);
   }
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
   if (SDL_Init(SDL_INIT_EVERYTHING) != 0) {
      std::cout << "SDL_Init Error: " << SDL_GetError() << std::endl;
      return 1;
   }
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

      SDL_Window *win = SDL_CreateWindow(
          "Hello World!", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
          pCodecCtx->width, pCodecCtx->height, 0);
      if (win == NULL) {
         std::cout << "SDL_CreateWindow Error: " << SDL_GetError() << std::endl;
         SDL_Quit();
         return 1;
      }
      renderer = SDL_CreateRenderer(win, -1, 1);
      texture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_YV12,
                                  SDL_TEXTUREACCESS_STREAMING, pCodecCtx->width,
                                  pCodecCtx->height);
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
   while(1) {
      while (av_read_frame(pFormatCtx, &packet) >= 0) {
         // Is this a packet from the video stream?
         if (packet.stream_index == videoStream) {
            // Decode video frame
            int frameFinished;
            avcodec_decode_video2(pCodecCtx, pFrame, &frameFinished, &packet);

            // Did we get a video frame?
            if (frameFinished) {
               sws_scale(sws_ctx, (uint8_t const * const *)pFrame->data,
                         pFrame->linesize, 0, pCodecCtx->height,
                         pFrameYUV->data, pFrameYUV->linesize);
               // YV12 Texture專用 正常顏色for YUV420
               SDL_UpdateYUVTexture(texture, &rect, pFrameYUV->data[0],
                                    pFrameYUV->linesize[0], pFrameYUV->data[1],
                                    pFrameYUV->linesize[1], pFrameYUV->data[2],
                                    pFrameYUV->linesize[2]);
               // SDL_UpdateTexture(texture, &rect,
              // pFrameYUV->data[0],pFrameYUV->linesize[0]);這個顏色怪怪的
               SDL_RenderClear(renderer);
               SDL_RenderCopy(renderer, texture, &rect, &rect);
               SDL_RenderPresent(renderer);
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
            case SDL_QUIT:
               quit = 1;
               SDL_Quit();
               exit(0);
               break;
            default:
               break;
         }
      }
avformat_seek_file( pFormatCtx, //format context
                            videoStream,//stream id
                            0,               //min timestamp
                            0,              //target timestamp
                            100,              //max timestamp
                            0); //AVSEEK_FLAG_ANY),//flags
   }
 
   if (videoStream != -1) {
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
