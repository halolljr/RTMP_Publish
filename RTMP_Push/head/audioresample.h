﻿#ifndef AUDIORESAMPLE_H
#define AUDIORESAMPLE_H


#include <string>
#include <vector>
#include <mutex>
#include <memory>
#include <iostream>

#ifdef __cplusplus
extern "C"
{
#endif
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include "libavutil/audio_fifo.h"
#include "libavutil/opt.h"
#include "libavutil/avutil.h"
#include "libswresample/swresample.h"
#include "libavutil/error.h"
#include "libavutil/frame.h"
#include "libavcodec/avcodec.h"
#ifdef __cplusplus
};
#endif
namespace LQF {
using::std::string;
using::std::vector;
using::std::shared_ptr;
//存储源和目标的音频格式、采样率、音频布局
typedef struct AudioResampleParams
{
    enum AVSampleFormat src_sample_fmt;
    enum AVSampleFormat dst_sample_fmt;
    int src_sample_rate = 0;
    int dst_sample_rate  = 0;
    uint64_t src_channel_layout = 0;
    uint64_t dst_channel_layout = 0;
    string logtag = "audioResample";
}AudioResampleParams;

extern std::ostream & operator<<(std::ostream & os, const AudioResampleParams & arp);

class AudioResampler {
public:
    AudioResampler() {};
    ~AudioResampler() {
        closeResample();
    }
    int InitResampler(const AudioResampleParams & arp);
    int SendResampleFrame(AVFrame *frame);
    int SendResampleFrame(uint8_t *in_pcm, const int in_size);
    shared_ptr<AVFrame> ReceiveResampledFrame(int desired_size = 0);
    int ReceiveResampledFrame(vector<shared_ptr<AVFrame>> & frames, int desired_size);

    inline int GetFifoCurSize() {
        std::lock_guard<std::mutex> lock(mutex_);
        return av_audio_fifo_size(audio_fifo_);
    }
    inline double GetFifoCurSizeInMs() {
        std::lock_guard<std::mutex> lock(mutex_);
        return av_audio_fifo_size(audio_fifo_) * 1000.0 / resample_params_.dst_sample_rate;
    }
    inline int64_t GetStartPts() const {return start_pts_;}
    inline int64_t GetCurPts() {std::lock_guard<std::mutex> lock(mutex_); return cur_pts_;}
    inline bool IsInit() {
        return is_init_;
    }
public:
    int closeResample();
    int initResampledData();
    shared_ptr<AVFrame> allocOutFrame(const int nb_samples);
    shared_ptr<AVFrame> getOneFrame(const int desired_size);

private:
    AudioResampler(const AudioResampler &) = delete;
    AudioResampler & operator= (const AudioResampler &) = delete;
    std::mutex mutex_;
    struct SwrContext *swr_ctx_ = nullptr;
    AudioResampleParams resample_params_;
    bool is_fifo_only = false;
    bool is_flushed = false;
    //准备一个 FIFO 缓冲区来存放重采样后的音频帧数据。
    AVAudioFifo *audio_fifo_ = nullptr;
    int64_t start_pts_ = AV_NOPTS_VALUE;
    int64_t cur_pts_ = AV_NOPTS_VALUE;

    uint8_t **resampled_data_ = nullptr;
    int resampled_data_size = 8192;
    int src_channels_ = 2;
    int dst_channels_ =2;
    int64_t total_resampled_num_ = 0;
    string logtag_;
    bool is_init_ = false;
    int dst_nb_samples = 1024;
    int max_dst_nb_samples = 1024;
    int dst_linesize = 0;
};
} // namespace audio_resample
#endif // AUDIORESAMPLE_H
