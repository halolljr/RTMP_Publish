#ifndef AACENCODER_H
#define AACENCODER_H

#ifdef __cplusplus
extern "C"
{
#endif
#ifndef __STDC_CONSTANT_MACROS
#define __STDC_CONSTANT_MACROS
#endif
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#ifdef __cplusplus
};
#endif
#include "mediabase.h"
#include "codecs.h"
class AACEncoder //: public AudioEncoder
{
public:
    AACEncoder();
    /**
     * @brief Init默认编码器需要FLTP模式（planar），麦克风的是s16（packed）
     * @param "sample_rate", 采样率，默认48000
     *        "channels", 通道数，默认2
     *        "bitrate", 比特率, 默认128*1024
     *        "channel_layout", 通道布局,  默认根据channels获取缺省的布局
     * @return
     */
    RET_CODE Init(const Properties &properties);
    virtual ~AACEncoder();
    virtual int Encode(AVFrame *frame, uint8_t* out, int out_len);
    virtual AVPacket * Encode(AVFrame *frame,int64_t pts,  const int flush = 0);
    virtual RET_CODE EncodeInput(const uint8_t *in, const uint32_t size);
    virtual RET_CODE EncodeInput(const AVFrame *frame);
    virtual RET_CODE EncodeOutput(AVPacket *pkt);
    virtual RET_CODE EncodeOutput(uint8_t *out, uint32_t &size);
    virtual uint32_t GetRate()			{ return ctx_->sample_rate?ctx_->sample_rate:8000;}
    virtual int get_sample_rate()		{ return ctx_->sample_rate;				}
    virtual int64_t get_bit_rate()		{ return ctx_->bit_rate;				}
    virtual uint64_t get_channel_layout()		{ return ctx_->channel_layout;				}
    //每通道需要的sample数量
    virtual uint32_t GetFrameSampleSize()
    {
        return ctx_->frame_size;
    }
    virtual uint32_t get_sample_fmt()
    {
        return ctx_->sample_fmt;
    }
    // 一帧数据总共需要的字节数 每个sample占用字节*channels*GetFrameSampleSize
    virtual uint32_t GetFrameByteSize()
    {
        return frame_byte_size_;
    }

    virtual int get_profile()
    {
        return ctx_->profile;
    }
    virtual int get_channels()
    {
        return ctx_->channels;
    }
    virtual int get_sample_format()
    {
        return ctx_->sample_fmt;
    }
    /// <summary>
    /// 为 AAC 编码后的原始数据生成一个 ADTS 头部（7 字节），用于打包成标准的 AAC 流或用于流式传输（如 .aac 文件、RTMP 传输等）。
    /// </summary>
    /// <param name="adts_header"></param>
    /// <param name="aac_length"></param>
    void GetAdtsHeader(uint8_t *adts_header, int aac_length)
    {
        uint8_t freqIdx = 0;    //0: 96000 Hz  3: 48000 Hz 4: 44100 Hz
        //ADTS 头中的采样率索引，不能直接写采样率，要写一个编号（标准定义）
        switch (ctx_->sample_rate)
        {
            case 96000: freqIdx = 0; break;
            case 88200: freqIdx = 1; break;
            case 64000: freqIdx = 2; break;
            case 48000: freqIdx = 3; break;
            case 44100: freqIdx = 4; break;
            case 32000: freqIdx = 5; break;
            case 24000: freqIdx = 6; break;
            case 22050: freqIdx = 7; break;
            case 16000: freqIdx = 8; break;
            case 12000: freqIdx = 9; break;
            case 11025: freqIdx = 10; break;
            case 8000: freqIdx = 11; break;
            case 7350: freqIdx = 12; break;
            default:
                LogError("can't support sample_rate:%d");
                freqIdx = 4;
            break;
        }
        //通道数（channel configuration）同样需要用一个特定的格式编码在 header 中
        uint8_t ch_cfg = ctx_->channels;
        //前 AAC 帧的 总长度 = 编码后的裸数据 + ADTS header 长度（7）
        uint32_t frame_length = aac_length + 7;

        adts_header[0] = 0xFF;   // 固定：syncword 的前8位 (0xFFF)
        adts_header[1] = 0xF1;   // syncword后4位 + MPEG-2/4 + layer + protection_absent
        adts_header[2] = ((ctx_->profile) << 6)      // profile: 通常是 AAC-LC (值为 1)
                        + (freqIdx << 2)     // 采样率索引
                        + (ch_cfg >> 2);     // 声道数前2位
        adts_header[3] = (((ch_cfg & 3) << 6) // 声道数后2位
                        + (frame_length  >> 11));   // 帧长度的高2位
        adts_header[4] = ((frame_length & 0x7FF) >> 3); // 帧长度中间8位
        adts_header[5] = (((frame_length & 7) << 5) + 0x1F);    // 帧长度最低3位 + 固定位
        adts_header[6] = 0xFC;   // 固定位 + buffer fullness = 0x7FF (表示未知)
    }
    AVCodecContext* GetCodecContext() {
        return ctx_;
    }
private:
    // 需要配置的参数
    int sample_rate_; // 默认 48000
    int channels_;    //    通道个数
    int bitrate_;    //    默认out_samplerate*3
    int channel_layout_;  //  默认AV_CH_LAYOUT_STEREO

    AVCodec 	*codec_;
    AVCodecContext	*ctx_;
    AVFrame         *frame_;

    //    int samplesSize_;
    //    int samplesNum_;
    AudioCodec::Type	type_;
    int			frame_byte_size_;      // 一帧的输入byte size
};

#endif // AACENCODER_H
