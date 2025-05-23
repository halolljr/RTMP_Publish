﻿#include "audioresample.h"
#include "dlog.h"
namespace LQF {
std::ostream & operator<<(std::ostream & os, const AudioResampleParams & arp) {
    const char *srcFmtStr = av_get_sample_fmt_name(arp.src_sample_fmt);
    const char *dstFmtStr = av_get_sample_fmt_name(arp.dst_sample_fmt);
    int srcChannels = av_get_channel_layout_nb_channels(arp.src_channel_layout);
    int dstChannels = av_get_channel_layout_nb_channels(arp.dst_channel_layout);
    os << "AudioResampleParams: \n"
       << "SRC: layout " << arp.src_channel_layout << ", channels "<< srcChannels
       << ", samplerate " << arp.src_sample_rate << ", fmt " << srcFmtStr
       << "\nDST: layout " << arp.dst_channel_layout << ", channels " << dstChannels
       << ", samplerate " << arp.dst_sample_rate << ", fmt " << dstFmtStr;
    return os;
}

/// <summary>
/// 先分配音频fifo作为最终存储音频数据的空间，之后判断是否需要重采样，判断后再开辟暂时存放重采样后的数据的空间
/// </summary>
/// <param name="arp"></param>
/// <returns></returns>
int AudioResampler::InitResampler(const AudioResampleParams & arp) {
    int ret = 0;
    resample_params_ = arp;
    logtag_ = resample_params_.logtag;
    /* fifo of output sample format */
    src_channels_ = av_get_channel_layout_nb_channels(resample_params_.src_channel_layout);
    dst_channels_ = av_get_channel_layout_nb_channels(resample_params_.dst_channel_layout);
    //分配一个音频 FIFO（先进先出缓冲区），用于存储目标格式的音频数据
	/*创建一个 先进先出（FIFO）音频缓冲区，格式为目标格式。
	1 表示初始容量为 1一个采样点，后面用 av_audio_fifo_write() 写入了更多数据，它会 自动扩容，。*/
    audio_fifo_ = av_audio_fifo_alloc(resample_params_.dst_sample_fmt, dst_channels_, 1);
    if(!audio_fifo_)
    {
        LogError("%s av_audio_fifo_alloc failed", logtag_.c_str());
        return -1;
    }
    //判断是否需要重采样
    //如果源和目标音频的采样格式、采样率、声道布局完全一致，则无需重采样。
    if (resample_params_.src_sample_fmt == resample_params_.dst_sample_fmt &&
            resample_params_.src_sample_rate == resample_params_.dst_sample_rate &&
            resample_params_.src_channel_layout == resample_params_.dst_channel_layout)
    {
        LogInfo("%s no resample needed, just use audio fifo",
                logtag_.c_str());
        is_fifo_only = true;
        return 0;
    }

    swr_ctx_ = swr_alloc();
    if(!swr_ctx_)
    {
        LogError("%s swr_alloc failed", logtag_.c_str());
        return -1;
    }

    //配置重采样参数
	//任一操作失败会返回非零值，此时记录错误并返回 -1。
    ret = 0;
    ret |= av_opt_set_int(swr_ctx_,        "in_channel_layout",  resample_params_.src_channel_layout, 0);
    ret |= av_opt_set_int(swr_ctx_,        "in_sample_rate",     resample_params_.src_sample_rate, 0);
    ret |= av_opt_set_sample_fmt(swr_ctx_, "in_sample_fmt",      resample_params_.src_sample_fmt, 0);

    ret |= av_opt_set_int(swr_ctx_,        "out_channel_layout", resample_params_.dst_channel_layout, 0);
    ret |= av_opt_set_int(swr_ctx_,        "out_sample_rate",    resample_params_.dst_sample_rate, 0);
    ret |= av_opt_set_sample_fmt(swr_ctx_, "out_sample_fmt",     resample_params_.dst_sample_fmt, 0);
    if(ret != 0) {
        LogError("av_opt_set_int failed");
        return -1;
    }

    //实际执行初始化，内部会检查你设定的参数是否合法，失败返回错误码。
    ret = swr_init(swr_ctx_);
    if (ret < 0) {
        LogError("%s  failed to initialize the resampling context.", logtag_.c_str());
        return ret;
    }

    //假定一帧的单个通道的采样数是1024（AAC编码器的一个通道的采样数就是1024）
    int src_nb_samples = 1024;
    //前置知识1：nb_samples/sample_rate：一帧的持续时间（仅考虑每个通道的采样点数），采样点数往往不是重采样的前置条件（除了以视频时钟为主的同步策略）
    //前置知识2：1/sample_rate：一个采样点的持续时间
    //计算原理：音频一帧数据通常是按固定的时间间隔采样的，采样率不同（例如：44.1 kHz 与 48 kHz）会导致每秒钟采样的数据点数不同。
    //根据时间相等：src_nb_samples/src_sample_rate==dst_nb_samples/dst_sample_rate
    //dst_nb_samples = src_nb_samples * (dst_sample_rate / src_sample_rate)
    max_dst_nb_samples = dst_nb_samples =
            av_rescale_rnd(src_nb_samples,
                           resample_params_.dst_sample_rate,
                           resample_params_.src_sample_rate, AV_ROUND_UP);

    if (initResampledData() < 0)
        return AVERROR(ENOMEM);
    is_init_ = true;
    LogInfo("%s init done ", logtag_.c_str());
    return 0;
}

int AudioResampler::closeResample()
{
    if (swr_ctx_)
        swr_free(&swr_ctx_);
    if (audio_fifo_) {
        av_audio_fifo_free(audio_fifo_);
        audio_fifo_ = nullptr;
    }
    if (resampled_data_)
        av_freep(&resampled_data_[0]);
    av_freep(&resampled_data_);
    return 0;
}

int AudioResampler::SendResampleFrame(AVFrame *frame)
{
    std::lock_guard<std::mutex> lock(mutex_);
    int src_nb_samples = 0;
    uint8_t **src_data = nullptr;
    if (frame)
    {
        src_nb_samples = frame->nb_samples;
        src_data = frame->extended_data;
        if (start_pts_ == AV_NOPTS_VALUE && frame->pts != AV_NOPTS_VALUE)
        {
            start_pts_ = frame->pts;
            cur_pts_ = frame->pts;
        }
    }
    else
    {
        is_flushed = true;
    }
    //特殊模式（只使用 FIFO，不做重采样），用于输入数据已经是目标格式，无需重采样
    if (is_fifo_only) {
        return src_data ? av_audio_fifo_write(audio_fifo_, (void **)src_data, src_nb_samples) : 0;
    }
    //当前 SwrContext 中未输出的延迟采样数
    int delay = swr_get_delay(swr_ctx_, resample_params_.src_sample_rate);
    dst_nb_samples = av_rescale_rnd(delay
                                    + src_nb_samples,
                                    resample_params_.dst_sample_rate ,
                                    resample_params_.src_sample_rate,
                                    AV_ROUND_UP);
    //   const int dst_nb_samples = av_rescale_rnd(src_nb_samples,
    //                                             resample_params_.dst_sample_rate,
    //                                             resample_params_.src_sample_rate,
    //                                             AV_ROUND_UP) + 20;
    //    dst_nb_samples = 941;
    
    //检查目标缓存空间够不够，重新分配
    if (dst_nb_samples > max_dst_nb_samples) {
        av_freep(&resampled_data_[0]);
        int ret = av_samples_alloc(resampled_data_, &dst_linesize, dst_channels_,
                               dst_nb_samples, resample_params_.dst_sample_fmt, 1);
        if (ret < 0) {
            LogError("av_samples_alloc failed");
            return 0;
        }
        max_dst_nb_samples = dst_nb_samples;
    }
    //执行重采样
    //如果 src_nb_samples 很大，而 dst_nb_samples 设置得较小，swr_convert 可能会根据目标缓冲区的大小分多次转换数据，直到所有源数据都被处理完。
    //返回的 nb_samples 是实际转换的样本数，而 dst_nb_samples 只是提供了目标缓冲区可以容纳的空间限制。
    //所以需要
    int nb_samples = swr_convert(swr_ctx_, resampled_data_, dst_nb_samples,
                                 (const uint8_t **)src_data, src_nb_samples);
    //获取缓冲区大小 & 写文件（调试）
    //第一个参数是输出参数，即每个通道对齐后的大小
    int dst_bufsize = av_samples_get_buffer_size(&dst_linesize, dst_channels_,
                                                     nb_samples, resample_params_.dst_sample_fmt, 1);
    // dump
    //有bug，可能写漏了部分数据（planar格式一般有多个通道）
    //可能也没有，因为传入了所有通道加起来的字节大小
    static FILE *s_swr_fp = fopen("swr.pcm","wb");
    fwrite(resampled_data_[0], 1, dst_bufsize, s_swr_fp);
    fflush(s_swr_fp);
    //  实际是FIFO的坑
    return av_audio_fifo_write(audio_fifo_, (void **)resampled_data_, nb_samples);
}

int AudioResampler::SendResampleFrame(uint8_t *in_pcm, const int in_size)
{
    if(!in_pcm)
    {
        is_flushed = true;
        return 0;
    }
    //创建AVFrame的智能指针,在该函数被调用完成后会被释放
    auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p)
    {if (p) av_frame_free(&p);});
    //设置AVFrame的参数
    frame->format = resample_params_.src_sample_fmt;
    frame->channel_layout = resample_params_.src_channel_layout;
    int ch = av_get_channel_layout_nb_channels(resample_params_.src_channel_layout);
    frame->nb_samples = in_size/ av_get_bytes_per_sample(resample_params_.src_sample_fmt) /ch;
    //将原始 PCM 数据指针 in_pcm 填入 frame->data[] 和 frame->linesize[] 等字段，让 FFmpeg 能识别并处理。
    avcodec_fill_audio_frame(frame.get(), ch, resample_params_.src_sample_fmt, in_pcm, in_size, 0);
    int ret = SendResampleFrame(frame.get());
    return ret;
}

shared_ptr<AVFrame> AudioResampler::ReceiveResampledFrame(int desired_size) {
    std::lock_guard<std::mutex> lock(mutex_);
    desired_size = desired_size == 0 ? av_audio_fifo_size(audio_fifo_) : desired_size;
    if (av_audio_fifo_size(audio_fifo_) < desired_size || desired_size == 0)
        return {};
    /* this call cannot identify the right time of flush, the caller should keep this state */
    return getOneFrame(desired_size);
}

int AudioResampler::ReceiveResampledFrame(vector<shared_ptr<AVFrame>> & frames,
                                          int desired_size)
{
    std::lock_guard<std::mutex> lock(mutex_);
    int ret = 0;
    desired_size = desired_size == 0 ? av_audio_fifo_size(audio_fifo_) : desired_size;
    do
    {
        //编码不等于解码播放那样子自由，必须按照固定采样点去适配编码器所需的采样点数（这也解释了为什么在绑定编码器【并非加码器】的时候会直接得到采样点个数）
        if (av_audio_fifo_size(audio_fifo_) < desired_size || desired_size == 0)
            break;
        auto frame = getOneFrame(desired_size);
        if (frame)
        {
            frames.push_back(frame);
        }
        else
        {
            ret = AVERROR(ENOMEM);
            break;
        }
    } while(true);

    if (is_flushed)
    {
        ret = AVERROR_EOF;
        frames.push_back(shared_ptr<AVFrame>()); // insert an empty frame
    }
    return ret;
}

/// <summary>
/// 内部先构建一个AVFrame，然后从fifo队列读取数据，并记录pts的累加
/// </summary>
/// <param name="desired_size"></param>
/// <returns></returns>
shared_ptr<AVFrame> AudioResampler::getOneFrame(const int desired_size)
{
    auto frame = allocOutFrame(desired_size);
    if (frame)
    {
        int ret = av_audio_fifo_read(audio_fifo_, (void **)frame->data, desired_size);
        if(ret <= 0) {
            LogError("av_audio_fifo_read failed");
        }
        frame->pts = cur_pts_;
        cur_pts_ += desired_size;
        total_resampled_num_ += desired_size;
    }
    return frame;
}

int AudioResampler::initResampledData()
{
	//1. 平面格式（如 AV_SAMPLE_FMT_FLTP）

	//	resampled_data_[0] = 左声道数据
	//	resampled_data_[1] = 右声道数据
	//2. 打包格式（如 AV_SAMPLE_FMT_S16）

	//	resampled_data_[0] = [L, R][L, R][L, R]....（交错存储）
	//	resampled_data_[1] = NULL（因为只有一个指针存所有数据）

    if (resampled_data_)
        //先释放内部 data[0] 指向的实际音频 buffer；
        av_freep(&resampled_data_[0]);
    //然后释放 resampled_data_ 自身这个指针数组。
    av_freep(&resampled_data_);
	/*分配一个指针数组 resampled_data_（类似 uint8_t* data[]）

		每个通道一个指针，共 dst_channels_ 个。

		为每个通道分配足够大的音频数据 buffer

		总大小由参数：dst_channels_* dst_nb_samples* bytes_per_sample 决定
        
         dst_linesize 的含义：每个声道所需的缓冲区大小（单位：字节）。
        (在 Packed 模式 下与 dst_linesize 相同；planar模式下：dst_linesize = dst_nb_samples * bytes_per_sample)

		bytes_per_sample 来源于 dst_sample_fmt*/
    int ret=  av_samples_alloc_array_and_samples(&resampled_data_, &dst_linesize, dst_channels_,
                                                 dst_nb_samples, resample_params_.dst_sample_fmt, 0);
    if (ret < 0)
        LogError("%s fail accocate audio resampled data buffer", logtag_.c_str());
    return ret;
}

/// <summary>
/// 分配并初始化一个AVFrame，参数设置为采样后的
/// </summary>
/// <param name="nb_samples"></param>
/// <returns></returns>
shared_ptr<AVFrame> AudioResampler::allocOutFrame(const int nb_samples)
{
    int ret;
    auto frame = shared_ptr<AVFrame>(av_frame_alloc(), [](AVFrame *p) {if (p) av_frame_free(&p);});
    if(!frame)
    {
        LogError("%s av_frame_alloc frame failed", logtag_.c_str());
        return {};
    }
    frame->nb_samples = nb_samples;
    frame->channel_layout = resample_params_.dst_channel_layout;
    frame->format = resample_params_.dst_sample_fmt;
    frame->sample_rate = resample_params_.dst_sample_rate;
    ret = av_frame_get_buffer(frame.get(), 0);
    if (ret < 0)
    {
        LogError("%s cannot allocate audio data buffer", logtag_.c_str());
        return {};
    }
    return frame;
}
}
