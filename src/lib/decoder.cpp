#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#ifdef __cplusplus
}
#endif
#include "../common/common_header.h"
#include "MyQueue.h"
#include "ptr_deleter.h"
#include "decoder.h"
#include <vector>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <signal.h>
#include <SDL2/SDL.h>
#include <SDL2/SDL_thread.h>
#include <SDL2/SDL_events.h>


Decoder::Decoder() {
   quit = 0;
   pictq.resize(VIDEO_PICTURE_QUEUE_SIZE);
   pthread_mutex_init(&pictq_mutex, NULL);
   pthread_cond_init(&pictq_cond, NULL);
}
int Decoder::decoder_init(AVCodecContext *codecCtx) {
   AVCodec *codec{nullptr};
   AVDictionary *optionsDict{nullptr};

   codec = avcodec_find_decoder(codecCtx->codec_id);
   if (!codec || (avcodec_open2(codecCtx, codec, &optionsDict) < 0)) {
      fprintf(stderr, "Unsupported codec!\n");
      return -1;
   }
   return 0;
}
int Decoder::video_decoder_init(AVCodecContext *codecCtx) {
   if (decoder_init(codecCtx) < 0)
      return -1;
   else
      pCodec = codecCtx;
   return 0;
}
int Decoder::audio_decoder_init(AVCodecContext *codecCtx) {
   if (decoder_init(codecCtx) < 0)
      return -1;
   else
      aCodec = codecCtx;
   return 0;
}
int Decoder::open_from_file(char *_file_name) {
   av_strlcpy(filename, _file_name, sizeof(filename));
   printf("open file:%s\n", filename);
   av_register_all();
   if (avformat_open_input(&pFormatCtx, filename, NULL, NULL) != 0) {
      printf("file not found\n");
      exit(-1);  // Couldn't open file
   }

   if (avformat_find_stream_info(pFormatCtx, NULL) < 0) return -1;
   av_dump_format(pFormatCtx, 0, filename, 0);
   pthread_create(&this->parse_tid, NULL, &Decoder::read_thread, (void *)this);
   return 0;
}
void *Decoder::read_thread(void *data) {
   printf("Read Thread:%lu\n", (long int)syscall(SYS_gettid));
   Decoder *d = (Decoder *)data;
   AVPacket pkt1;
   std::unique_ptr<AVPacket, nodeleter<AVPacket>> packet(&pkt1,
                                                         nodeleter<AVPacket>());
   if (d->init(d->pFormatCtx) < 0) {
      printf("init failed\n");
      goto fail;
   }
   printf("init finish!!\n");
   for (;;) {
      if (d->quit) {
         break;
      }

      // seek stuff goes here
      if (d->audioq.size > MAX_AUDIOQ_SIZE || d->videoq.size > MAX_VIDEOQ_SIZE)
      // if(is->audioq.nb_packets >= 1000)
      // if (is->videoq.nb_packets >= 1000)
      {
         usleep(10);
         continue;
      }

      if (av_read_frame(d->pFormatCtx, packet.get()) < 0) {
         if (d->pFormatCtx->pb->error == 0) {
            printf("Read no yet!!\n");
            usleep(100);  // no error; wait for user input
            continue;
         } else {
            break;
         }
      }
      // Is this a packet from the video stream?
      if (packet->stream_index == d->videoStream) {
         // continue;
         // printf("read video packets!!\n");
         d->videoq.packet_queue_put(packet);
         // printf("Video Queue length:%d\n",d->videoq.nb_packets);
      } else if (packet->stream_index == d->audioStream) {
         // continue;
         // printf("read audio packets!!\n");
         //d->audioq.packet_queue_put(packet);
         // printf("Audio Queue length:%d\n",is->audioq.nb_packets);
      } else {
         av_free_packet(packet.get());
         packet.reset(nullptr);
      }
   }
   packet.reset(nullptr);
   // pthread_join(d->audio_tid, NULL);

   printf("wait video_decode_thread!!\n");
   pthread_join(d->video_tid, NULL);
   printf("exit read_thread!!\n");

fail:
   if (1) {
      d->quit = 1;
   }
   return 0;
}
int Decoder::stream_component_open(int stream_index) {
   AVCodecContext *codecCtx;

   if (stream_index < 0 || stream_index >= (int)pFormatCtx->nb_streams) {
      return -1;
   }

   codecCtx = pFormatCtx->streams[stream_index]->codec;
   decoder_init(codecCtx);
   switch (codecCtx->codec_type) {
      case AVMEDIA_TYPE_AUDIO:
         audioStream = stream_index;
         audio_st = pFormatCtx->streams[stream_index];
         aCodec = codecCtx;
         audio_buf_size = 0;
         audio_buf_index = 0;
         printf("create audio thread!!\n");
         // pthread_create(&this->audio_tid, NULL,
         // &Decoder::audio_decode_thread,
         //               (void *)this);
         break;
      case AVMEDIA_TYPE_VIDEO:
         videoStream = stream_index;
         video_st = pFormatCtx->streams[stream_index];
         pCodec = codecCtx;
         printf("create video thread!!\n");
         pthread_create(&this->video_tid, NULL, &Decoder::video_decode_thread,
                        (void *)this);
         break;
      default:
         break;
   }
   printf("find codec end!!\n");
   return 0;
}
int Decoder::init(AVFormatContext *formatcontext) {
   int video_index = -1;
   int audio_index = -1;
   for (int i = 0; i < (int)pFormatCtx->nb_streams; i++) {
      if (formatcontext->streams[i]->codec->codec_type == AVMEDIA_TYPE_VIDEO &&
          video_index < 0) {
         video_index = i;
      }
      if (pFormatCtx->streams[i]->codec->codec_type == AVMEDIA_TYPE_AUDIO &&
          audio_index < 0) {
         audio_index = i;
      }
   }
   if (audio_index >= 0) {
      printf("init for audio!!\n");
      stream_component_open(audio_index);
      printf("init audio end!!\n");
   }
   if (video_index >= 0) {
      printf("init for video!!\n");
      stream_component_open(video_index);
   }
   printf("audio_index:%d,video_index%d\n", audioStream, videoStream);
   if (videoStream < 0 || audioStream < 0) {
      fprintf(stderr, "%s: could not open codecs\n", filename);
      return -1;
   }
   return 0;
}
int Decoder::audio_resample(
    AVSampleFormat infomat,
    std::unique_ptr<AVFrame, mydeleter<AVFrame>> &inFrame, int in_size,
    AVSampleFormat outformat, std::shared_ptr<AVFrame> &outFrame,
    int *out_size) {
   if (!audio_swr) {
      uint64_t in_channel_layout =
          av_get_default_channel_layout(aCodec->channels);
      uint64_t out_channel_layout =
          av_get_default_channel_layout(aCodec->channels);
      audio_swr = swr_alloc_set_opts(
          NULL, out_channel_layout, outformat, aCodec->sample_rate,
          in_channel_layout, aCodec->sample_fmt, aCodec->sample_rate, 0, NULL);

      swr_init(audio_swr);
   }
   if (audio_swr) {
      int nb_samples = inFrame->nb_samples;
      int channels = inFrame->channels;
      int dst_buf_size = 0;
      int bytes_per_sample = av_get_bytes_per_sample(outformat);
      dst_buf_size = nb_samples * bytes_per_sample * channels;
      if (!g_pFrameAudio) {
         printf("allocate memory\n");
         g_pFrameAudio.reset(av_frame_alloc(), mydeleter<AVFrame>());

         printf("g_pFrameAudio address:%p\n",g_pFrameAudio.get());
         g_pFrameAudio->data[0] = (uint8_t *)av_malloc(dst_buf_size);
      }
      // printf("shared count1:%d!!\n",g_pFrameAudio.use_count());
      outFrame = g_pFrameAudio;
      // printf("shared count2:%d!!\n",g_pFrameAudio.use_count());
      assert(outFrame->data[0]);
      avcodec_fill_audio_frame((AVFrame *)outFrame.get(), channels, outformat,
                               outFrame->data[0], dst_buf_size, 0);
      // printf("start resample\n");
      int ret, out_count;
      out_count = dst_buf_size / channels / av_get_bytes_per_sample(outformat);

      ret =
          swr_convert(audio_swr, outFrame->data, out_count,
                      const_cast<const uint8_t **>(inFrame->data), nb_samples);
      if (ret < 0) assert(0);
      outFrame->linesize[0] =
          ret * av_get_bytes_per_sample(outformat) * channels;
      *out_size = ret * av_get_bytes_per_sample(outformat) * channels;
   }
   return 0;
}
int Decoder::decode_audio_frame(
    std::unique_ptr<AVPacket, nodeleter<AVPacket>> &packet,
    std::unique_ptr<uint8_t, nodeleter<uint8_t>> &outbuff) {
   if (!aCodec)
      fprintf(stderr, "No Codec allocate!!\n");
   else {
      int len1 = 0, data_size = 0;
      if (!decoded_frame) {
         decoded_frame = std::move(std::unique_ptr<AVFrame,mydeleter<AVFrame>>(av_frame_alloc(),mydeleter<AVFrame>()));
         printf("decoded_frame address:%p\n",decoded_frame.get());
      }
      while (packet->size > 0) {
         int got_frame = 0;
         len1 = avcodec_decode_audio4(aCodec, decoded_frame.get(), &got_frame,
                                      packet.get());

         if (len1 < 0) {
            break;
         }
         if (!got_frame) {
            /* stop sending empty packets if the decoder is finished */

            continue;
         }
         if (got_frame) {
            data_size = av_samples_get_buffer_size(NULL, aCodec->channels,
                                                   decoded_frame->nb_samples,
                                                   aCodec->sample_fmt, 1);
            if (data_size < 0) {
               /* This should not occur, checking just for paranoia */
               fprintf(stderr, "Failed to calculate data size\n");
               continue;
               // exit(1);
            }
            std::shared_ptr<AVFrame> out_Frame(nullptr, mydeleter<AVFrame>());
            // printf("start resample!!\n");
            
            if (aCodec->sample_fmt != AV_SAMPLE_FMT_S16) {
               audio_resample(aCodec->sample_fmt, decoded_frame,
                              decoded_frame->linesize[0], AV_SAMPLE_FMT_S16,
                              out_Frame, &data_size);

            } else {
               out_Frame = std::move(decoded_frame);
            }
            // printf("outbuff address:%x,out_Frame->data[0]
            // address:%x\n",outbuff.get(),out_Frame.get());
            //printf("shared count:%d!!\n",out_Frame.use_count());
            
            memcpy(outbuff.get(), out_Frame->data[0], data_size);

            return data_size;
         }
         packet->size -= len1;
         packet->data += len1;
      }
   }
   return 0;
}
void *Decoder::video_decode_thread(void *data) {
   Decoder *d = (Decoder *)data;
   std::unique_ptr<AVFrame, mydeleter<AVFrame>> pFrame(av_frame_alloc(),
                                                       mydeleter<AVFrame>());
   
      printf("pFrame address:%p\n",pFrame.get());
   AVPacket pkt1;
   std::unique_ptr<AVPacket, nodeleter<AVPacket>> packet(&pkt1,
                                                         nodeleter<AVPacket>());

   // printf("Video Thread:%lu\n", (long int)syscall(SYS_gettid));
   // printf("start decode!!\n");
   for (;;) {
      // printf("Video queue size:%d\n", d->videoq.nb_packets);
      if (d->quit) break;
      // printf("start get packet!!\n");
      if (d->videoq.packet_queue_get(packet, 1) < 0) {
         // means we quit getting packets
         printf("video packet get error from queue\n");
         break;
      }
      // printf("end get packet!!\n");
      // Decode video frame
      // if (!d->pCodec) d->pCodec = d->video_st->codec;
      while (packet->size > 0) {
         if (d->decode_video_frame(packet, pFrame) > 0) {
            if (d->queue_picture(pFrame) < 0) {
               printf("decode and queuing frame error\n");
               break;
            }
         }
      }
      av_free_packet(packet.get());
   }
   packet.reset(nullptr);
   printf("exit video_decode_thread!!\n");
   return 0;
}
int Decoder::decode_video_frame(
    std::unique_ptr<AVPacket, nodeleter<AVPacket>> &packet,
    std::unique_ptr<AVFrame, mydeleter<AVFrame>> &outFrame) {
   // return 1 means packet has remain frames to decoed
   // retrun 2 means packet totally decode all frames
   if (!pCodec)
      fprintf(stderr, "No Codec allocate!!\n");
   else {
      int frameFinished = 0, len1 = 0;

      /* the picture is allocated by the decoder, no need to free it */

      len1 = avcodec_decode_video2(pCodec, outFrame.get(), &frameFinished,
                                   packet.get());

      if (len1 < 0) {
         fprintf(stderr, "Error while decoding frame\n");
         return -1;
      }
      if (packet->data) {
         packet->size -= len1;
         packet->data += len1;
      }
      // Did we get a video frame?
      if (frameFinished) {
         return len1;
      }
      return 0;  // means no frame decode
   }
   return -1;  // means deocode error
}
int Decoder::get_picture(
    std::shared_ptr<AVFrame> &outFrame) {
   while (pictq_size == 0) {
      return -1;
   }
   std::unique_ptr<VideoPicture, nodeleter<VideoPicture>> vp(
       &pictq[pictq_rindex], nodeleter<VideoPicture>());
   if (vp->pFrameYUV)
   {
      memcpy(outFrame.get(),vp->pFrameYUV.get(),sizeof(AVFrame));
      //*outFrame = *(vp->pFrameYUV);
   }
   if (++pictq_rindex == VIDEO_PICTURE_QUEUE_SIZE) {
      pictq_rindex = 0;
   }
   pthread_mutex_lock(&pictq_mutex);
   pictq_size--;
   pthread_cond_signal(&pictq_cond);
   pthread_mutex_unlock(&pictq_mutex);
   return 0;
}
int Decoder::queue_picture(
    std::unique_ptr<AVFrame, mydeleter<AVFrame>> &pFrame) {
   int dst_pix_fmt;
   pthread_mutex_lock(&pictq_mutex);

   while (pictq_size >= VIDEO_PICTURE_QUEUE_SIZE && !quit) {
      pthread_cond_wait(&pictq_cond, &pictq_mutex);
   }
   pthread_mutex_unlock(&pictq_mutex);
   if (quit) return -1;

   std::unique_ptr<VideoPicture, nodeleter<VideoPicture>> vp(
       &pictq[pictq_windex], nodeleter<VideoPicture>());
   // VideoPicture *vp = &pictq[pictq_windex];
   if (this->g_pFrameYUV == nullptr) {
      printf("g_pFrameYUV allocated\n");

      this->g_pFrameYUV =
          std::shared_ptr<AVFrame>(av_frame_alloc(), mydeleter<AVFrame>());
      printf("g_pFrameYUV address:%p\n",g_pFrameYUV.get());
      vp->pFrameYUV = this->g_pFrameYUV;
   } else {
      // printf("no allocated\n");
      vp->pFrameYUV = this->g_pFrameYUV;
      if (g_pFrameYUV == nullptr) {
         printf("g_pFrameYUV Error\n");
         exit(-1);
      }
   }
   if (vp->pFrameYUV) {
      // printf("queue picture!!!\n");
      if (!sws_ctx) {
         sws_ctx =
             sws_getContext(video_st->codec->width, video_st->codec->height,
                            video_st->codec->pix_fmt, video_st->codec->width,
                            video_st->codec->height, PIX_FMT_YUV420P,
                            SWS_BILINEAR, NULL, NULL, NULL);
      }
      if (sws_ctx) {
         // SDL_LockMutex(is->pictq_mutex);
         dst_pix_fmt = PIX_FMT_YUV420P;

         if (buffer == nullptr) {
            printf("memory allocate!!\n");
            int numBytes =
                avpicture_get_size(PIX_FMT_YUV420P, video_st->codec->width,
                                   video_st->codec->height);
            buffer.reset((uint8_t *)av_malloc(numBytes * sizeof(uint8_t)),
                         mydeleter<uint8_t>());
         }
         avpicture_fill((AVPicture *)vp->pFrameYUV.get(), buffer.get(),
                        PIX_FMT_YUV420P, video_st->codec->width,
                        video_st->codec->height);

         sws_scale(sws_ctx, (uint8_t const *const *)pFrame->data,
                   pFrame->linesize, 0, video_st->codec->height,
                   vp->pFrameYUV->data, vp->pFrameYUV->linesize);

         // Convert the image into YUV format that SDL uses

         // SDL_UnlockMutex(is->pictq_mutex);
      }

      if (++pictq_windex == VIDEO_PICTURE_QUEUE_SIZE) {
         pictq_windex = 0;
      }

      // printf("pictq_size:%d,pictq_windex:%d\n",pictq_size,pictq_windex);
      pthread_mutex_lock(&pictq_mutex);
      pictq_size++;
      pthread_cond_signal(&pictq_cond);
      pthread_mutex_unlock(&pictq_mutex);
   }
   return 0;
}
// static pthread_mutex_t audio_mutex;
// static pthread_cond_t audio_cond;
/*void *Decoder::audio_decode_thread(void *data) {
   printf("audio Thread:%lu\n", (long int)syscall(SYS_gettid));
   // pthread_mutex_init(&audio_mutex,NULL);
   // pthread_cond_init(&audio_cond,NULL);
   for (;;) {
      // printf("Sleep Start!!\n");
      //      pthread_mutex_lock(&audio_mutex);
      //      pthread_cond_wait(&audio_cond,&audio_mutex);
      //      pthread_mutex_unlock(&audio_mutex);
      sleep(100);
      // printf("Slepp End!!\n");
   }
   return 0;
}*/

Decoder::~Decoder() {}
int Decoder::deinit() {
   avcodec_close(aCodec);
   avcodec_close(pCodec);
   avformat_close_input(&pFormatCtx);
   videoq.force_release_lock();
   audioq.force_release_lock();
   g_pFrameAudio.reset();
   g_pFrameYUV.reset();
   buffer.reset();
   pictq.pop_back();
   decoded_frame.reset(nullptr);
   pict_Frame.reset();
   return 0;
}
