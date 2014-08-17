#ifndef DECODER_H
#define DECODER_H
#ifdef __cplusplus
extern "C" {
#endif
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/avstring.h>
#include <libavutil/time.h>
#include <unistd.h>
#ifdef __cplusplus
}
#endif
#include "../common/common_header.h"
#include "ptr_deleter.h"
#include "MyQueue.h"
#include <vector>
#define SDL_AUDIO_BUFFER_SIZE 1024
#define MAX_AUDIO_FRAME_SIZE 192000
#define MAX_AUDIOQ_SIZE (5 * 16 * 1024)
#define MAX_VIDEOQ_SIZE (5 * 256 * 1024)
#define VIDEO_PICTURE_QUEUE_SIZE 1
typedef struct VideoPicture {
   double pts;
   std::shared_ptr<AVFrame> pFrameYUV;
   int width, height;
   int allocated;
} VideoPicture;

class Decoder {
  public:
   pthread_t parse_tid{0};
   pthread_t video_tid{0};
   pthread_t audio_tid{0};
   Decoder();
   ~Decoder();
   // int audio_decode_frame();
   static int decode_interrupt_cb(void *);
   static void *read_thread(void *);
   // static void *audio_decode_thread(void *);
   static void *video_decode_thread(void *);
   int video_decoder_init(AVCodecContext *);
   int audio_decoder_init(AVCodecContext *);
   int decoder_init(AVCodecContext *);
   int decode_video_frame(std::unique_ptr<AVPacket, nodeleter<AVPacket>> &,
                          std::unique_ptr<AVFrame, mydeleter<AVFrame>> &);
   int decode_audio_frame(std::unique_ptr<AVPacket, nodeleter<AVPacket>> &,
                          std::unique_ptr<uint8_t, nodeleter<uint8_t>> &);
   int audio_resample(AVSampleFormat infomat,
                      std::unique_ptr<AVFrame, mydeleter<AVFrame>> &, int,
                      AVSampleFormat, std::shared_ptr<AVFrame> &, int *);
   int stream_component_open(int);
   int open_from_file(char *);
   int queue_picture(std::unique_ptr<AVFrame, mydeleter<AVFrame>> &);
   int get_picture(std::shared_ptr<AVFrame> &);
   AVStream *GetVideoStream() {
      while (!video_st) usleep(10);
      return video_st;
   }

   AVStream *GetAudioStream() {
      while (!audio_st) usleep(10);
      return audio_st;
   }
   int deinit();
   int quit{0};
   uint8_t audio_buf[(MAX_AUDIO_FRAME_SIZE * 3) / 2]{""};
   MyQueue audioq;
   MyQueue videoq;
   pthread_cond_t pictq_cond;
   pthread_mutex_t pictq_mutex;
   std::shared_ptr<AVFrame> g_pFrameYUV{nullptr};    // for image covert
   std::shared_ptr<AVFrame> g_pFrameAudio{nullptr};  // for audio resampling
   std::shared_ptr<uint8_t> buffer{nullptr};
   std::unique_ptr<AVFrame, mydeleter<AVFrame>> decoded_frame{nullptr};
   std::shared_ptr<AVFrame> pict_Frame{nullptr};
  private:
   int init(AVFormatContext *);
   AVCodecContext *aCodec{nullptr};
   AVCodecContext *pCodec{nullptr};
   bool videoCodec_flag{false};
   bool audioCodec_flag{false};
   AVFormatContext *pFormatCtx{nullptr};
   int videoStream{0};
   int audioStream{0};
   AVStream *audio_st{nullptr};
   AVStream *video_st{nullptr};
   unsigned int audio_buf_size{0};
   unsigned int audio_buf_index{0};
   std::vector<VideoPicture> pictq;
   int pictq_size{0};
   int pictq_rindex{0};
   int pictq_windex{0};
   char filename[1024];
   SwsContext *sws_ctx{nullptr};
   SwrContext *audio_swr{nullptr};
};
#endif
