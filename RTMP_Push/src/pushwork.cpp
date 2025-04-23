﻿#include "pushwork.h"
#include "avtimebase.h"

namespace LQF {
PushWork::PushWork()
{

}

PushWork::~PushWork()
{
    if(audio_capturer_)
        delete audio_capturer_;
    if(audio_encoder_)
        delete audio_encoder_;
    if(video_capturer)
        delete video_capturer;

    if(aac_buf_)
        delete [] aac_buf_;
    if(audio_resampler_)
        delete audio_resampler_;
    if(video_encoder_)
        delete video_encoder_;
    if(rtmp_pusher)
        delete rtmp_pusher;

    if(video_out_sdl)
        delete video_out_sdl;

    if(h264_fp_)
        fclose(h264_fp_);
    if(aac_fp_)
        fclose(aac_fp_);
    if(pcm_flt_fp_)
        fclose(pcm_flt_fp_);
    if(pcm_s16le_fp_)
        fclose(pcm_s16le_fp_);
}
/**
 * @brief PushWork::Init
 * @param properties
 * @return
 */
RET_CODE PushWork::Init(const Properties &properties)
{
    // 音频test模式
    audio_test_ = properties.GetProperty("audio_test", 0);
    input_pcm_name_ = properties.GetProperty("input_pcm_name", "input_48k_2ch_s16.pcm");

    // 麦克风采样属性
    mic_sample_rate_ = properties.GetProperty("mic_sample_rate", 48000);
    mic_sample_fmt_ = properties.GetProperty("mic_sample_fmt", AV_SAMPLE_FMT_S16);
    mic_channels_ = properties.GetProperty("mic_channels", 2);

    // 音频编码参数
    audio_sample_rate_ = properties.GetProperty("audio_sample_rate", mic_sample_rate_);
    audio_bitrate_ = properties.GetProperty("audio_bitrate", 128*1024);
    audio_channels_ = properties.GetProperty("audio_channels", mic_channels_);
    audio_ch_layout_ = av_get_default_channel_layout(audio_channels_);    // 由audio_channels_决定

    // 视频test模式
    video_test_ = properties.GetProperty("video_test", 0);
    input_yuv_name_ = properties.GetProperty("input_yuv_name", "input_1280_720_420p.yuv");

    // 桌面录制属性
    desktop_x_ = properties.GetProperty("desktop_x", 0);
    desktop_y_ = properties.GetProperty("desktop_y", 0);
    desktop_width_  = properties.GetProperty("desktop_width", 1920);
    desktop_height_ = properties.GetProperty("desktop_height", 1080);
    desktop_format_ = properties.GetProperty("desktop_pixel_format", AV_PIX_FMT_YUV420P);
    desktop_fps_ = properties.GetProperty("desktop_fps", 24);

    // 视频编码属性
    video_width_  = properties.GetProperty("video_width", desktop_width_);     // 宽
    video_height_ = properties.GetProperty("video_height", desktop_height_);   // 高
    video_fps_ = properties.GetProperty("video_fps", desktop_fps_);             // 帧率
    video_gop_ = properties.GetProperty("video_gop", video_fps_);
    video_bitrate_ = properties.GetProperty("video_bitrate", 1024*1024);   // 先默认1M fixedme
    video_b_frames_ = properties.GetProperty("video_b_frames", 0);   // b帧数量

    // rtmp推流
    rtmp_url_ = properties.GetProperty("rtmp_url", "");
    if(rtmp_url_ == "")
    {
        LogError("rtmp url is null");
        return RET_FAIL;
    }
    rtmp_debug_ = properties.GetProperty("rtmp_debug", 0);

    if(1 == rtmp_debug_)
    {
        video_out_sdl = new VideoOutSDL();  // 显示用的
        if(!video_out_sdl)
        {
            LogError("new VideoOutSDL() failed");
            return RET_FAIL;
        }
        Properties yuv_out_properties;
        yuv_out_properties.SetProperty("video_width", desktop_width_);
        yuv_out_properties.SetProperty("video_height", desktop_height_);
        yuv_out_properties.SetProperty("win_x", 200);
        yuv_out_properties.SetProperty("win_title", "push video display");
        if(video_out_sdl->Init(yuv_out_properties) != RET_OK)
        {
            LogError("video_out_sdl Init failed");
            return RET_FAIL;
        }
    }

    // 先启动RTMPPusher
    rtmp_pusher = new RTMPPusher();     // 启动后线程推流的线程就启动了，带了线程
    if(!rtmp_pusher)
    {
        LogError("new RTMPPusher() failed");
        return RET_FAIL;
    }

    if(!rtmp_pusher->Connect(rtmp_url_))
    {
        LogError("rtmp_pusher connect() failed");
        return RET_FAIL;
    }

    // 初始化publish time
    AVPublishTime::GetInstance()->Rest();   // 推流打时间戳的问题

    // 设置音频编码器，先音频捕获初始化
    audio_encoder_ = new AACEncoder();
    if(!rtmp_pusher)
    {
        LogError("new AACEncoder() failed");
        return RET_FAIL;
    }
    Properties  aud_codec_properties;
    aud_codec_properties.SetProperty("sample_rate", audio_sample_rate_);
    aud_codec_properties.SetProperty("channels", audio_channels_);
    aud_codec_properties.SetProperty("bitrate", audio_bitrate_);
    if(audio_encoder_->Init(aud_codec_properties) != RET_OK)
    {
        LogError("AACEncoder Init failed");
        return RET_FAIL;
    }
    aac_buf_ = new uint8_t[AAC_BUF_MAX_LENGTH];
    aac_fp_ = fopen("push_dump.aac", "wb");
    if(!aac_fp_)
    {
        LogError("fopen push_dump.aac failed");
        return RET_FAIL;
    }
    // 音频重采样，捕获的PCM数据是s16交错模式, 做编码的时候float棋盘格格式的(fltp)
    // 1-快速掌握音视频开发基础知识.pdf
    audio_resampler_ = new AudioResampler();
    AudioResampleParams aud_params;
    aud_params.logtag = "[audio-resample]";
    //S16e
    aud_params.src_sample_fmt = (AVSampleFormat)mic_sample_fmt_;
    //FLTP
    aud_params.dst_sample_fmt = (AVSampleFormat)audio_encoder_->get_sample_format();
    aud_params.src_sample_rate = mic_sample_rate_;
    aud_params.dst_sample_rate = audio_encoder_->get_sample_rate();
    aud_params.src_channel_layout = av_get_default_channel_layout(mic_channels_);
    aud_params.dst_channel_layout = audio_encoder_->get_channel_layout();
    aud_params.logtag = "audio-resample-encode";
    audio_resampler_->InitResampler(aud_params);

    // 视频编码
    video_encoder_ = new H264Encoder();
    Properties  vid_codec_properties;
    vid_codec_properties.SetProperty("width", video_width_);
    vid_codec_properties.SetProperty("height", video_height_);
    vid_codec_properties.SetProperty("fps", video_fps_);
    vid_codec_properties.SetProperty("b_frames", video_b_frames_);
    vid_codec_properties.SetProperty("bitrate", video_bitrate_);
    vid_codec_properties.SetProperty("gop", video_gop_);
    if(video_encoder_->Init(vid_codec_properties) != RET_OK)
    {
        LogError("H264Encoder Init failed");
        return RET_FAIL;
    }
    h264_fp_ = fopen("push_dump.h264", "wb");
    if(!h264_fp_)
    {
        LogError("fopen push_dump.h264 failed");
        return RET_FAIL;
    }
    //在 RTMP 协议里，推送的不是裸流，而是按 FLV 封装格式打包的数据。
    // 为什么 RTMP 推流一开始要发送 Metadata
    // 为了让接收端（播放器、服务器）“知道”后续数据的结构和格式，也就是告诉它：“我要开始播放啦，这就是我的配置！”
    FLVMetadataMsg *metadata = new FLVMetadataMsg();
    // 设置视频相关
    metadata->has_video = true; //是否有视频
    metadata->width = video_encoder_->get_width();  //视频宽
    metadata->height = video_encoder_->get_height();    //视频高
    metadata->framerate = video_encoder_->get_framerate();  //视频帧率
    metadata->videodatarate = video_encoder_->get_bit_rate();   //视频码率（kbps）
    // 设置音频相关
    metadata->has_audio = true; //是否有音频
    metadata->channles = audio_encoder_->get_channels();    //声道数
    metadata->audiosamplerate = audio_encoder_->get_sample_rate();  //采样率
    metadata->audiosamplesize = 16; //采样位数
    metadata->audiodatarate = 256;   //音频码率
    metadata->pts = 0;
    rtmp_pusher->Post(RTMP_BODY_METADATA, metadata, false);

    // 设置音频pts的间隔（和nb_samples/sample_rate（秒为单位）是一样的，只不过下面的用了毫秒为单位）
    double audio_frame_duration = 1000.0/audio_encoder_->get_sample_rate() *audio_encoder_->GetFrameSampleSize();
    LogInfo("audio_frame_duration:%lf", audio_frame_duration);
    AVPublishTime::GetInstance()->set_audio_frame_duration(audio_frame_duration);
    AVPublishTime::GetInstance()->set_audio_pts_strategy(AVPublishTime::PTS_RECTIFY);//帧间隔矫正
    // 设置音频捕获
    audio_capturer_ = new AudioCapturer();
    Properties  aud_cap_properties;
    aud_cap_properties.SetProperty("audio_test", 1);
    aud_cap_properties.SetProperty("input_pcm_name", input_pcm_name_);
    if(audio_capturer_->Init(aud_cap_properties) != RET_OK)
    {
        LogError("AACEncoder Init failed");
        return RET_FAIL;
    }
    //这个地方传入 this 指针，是因为你用的是 std::bind 来绑定类的成员函数，而成员函数在调用时必须依赖一个对象实例 —— 也就是你当前的 PushWork 对象
    audio_capturer_->AddCallback(std::bind(&PushWork::PcmCallback, this,
                                           std::placeholders::_1,
                                           std::placeholders::_2));
    if(audio_capturer_->Start() != RET_OK)
    {
        LogError("AudioCapturer Start failed");
        return RET_FAIL;
    }

    // 设置视频pts的间隔
    double video_frame_duration = 1000.0 / video_encoder_->get_framerate();
    LogInfo("video_frame_duration:%lf", video_frame_duration);
    AVPublishTime::GetInstance()->set_video_frame_duration(video_frame_duration);
    AVPublishTime::GetInstance()->set_video_pts_strategy(AVPublishTime::PTS_RECTIFY);//帧间隔矫正
    video_capturer = new VideoCapturer();
    Properties  vid_cap_properties;
    vid_cap_properties.SetProperty("video_test", 1);
    vid_cap_properties.SetProperty("input_yuv_name", input_yuv_name_);
    vid_cap_properties.SetProperty("width", desktop_width_);
    vid_cap_properties.SetProperty("height", desktop_height_);
    if(video_capturer->Init(vid_cap_properties) != RET_OK)
    {
        LogError("VideoCapturer Init failed");
        return RET_FAIL;
    }
    video_nalu_buf = new uint8_t[VIDEO_NALU_BUF_MAX_SIZE];

    video_capturer->AddCallback(std::bind(&PushWork::YuvCallback, this,
                                          std::placeholders::_1,
                                          std::placeholders::_2));
    if(video_capturer->Start() != RET_OK)
    {
        LogError("VideoCapturer Start failed");
        return RET_FAIL;
    }
    return RET_OK;
}

void PushWork::DeInit()
{
    if(audio_capturer_) {
        delete audio_capturer_;
        audio_capturer_ = NULL;
    }
    if(audio_encoder_) {
        delete audio_encoder_;
        audio_encoder_ = NULL;
    }
    if(video_capturer) {
        delete video_capturer;
        video_capturer = NULL;
    }
    if(audio_resampler_) {
        delete audio_resampler_;
        audio_resampler_ = NULL;
    }
    if(video_encoder_) {
        delete video_encoder_;
        video_encoder_ = NULL;
    }
    if(rtmp_pusher) {
        delete rtmp_pusher;
        rtmp_pusher = NULL;
    }

    if(video_out_sdl) {
        delete video_out_sdl;
        video_out_sdl = NULL;
    }

    if(h264_fp_) {
        fclose(h264_fp_);
        h264_fp_ = NULL;
    }
    if(aac_fp_) {
        fclose(aac_fp_);
        aac_fp_ = NULL;
    }
    if(pcm_flt_fp_) {
        fclose(pcm_flt_fp_);
        pcm_flt_fp_ = NULL;
    }
    if(pcm_s16le_fp_) {
        fclose(pcm_s16le_fp_);
        pcm_s16le_fp_ = NULL;
    }
}
void PushWork::AudioCallback(NaluStruct *nalu_data)
{

}

void PushWork::VideoCallback(NaluStruct *nalu_data)
{

}

void PushWork::PcmCallback(uint8_t *pcm, int32_t size)
{
    if(!pcm_s16le_fp_)
    {
        pcm_s16le_fp_ = fopen("push_dump_s16le.pcm", "wb");
    }
    if(pcm_s16le_fp_)
    {
        // ffplay -ar 48000 -channels 2 -f s16le  -i push_dump_s16le.pcm
//        fwrite(pcm, 1, size, pcm_s16le_fp_);
//        fflush(pcm_s16le_fp_);
    }

    if(need_send_audio_spec_config)
    {
        need_send_audio_spec_config = false;
        AudioSpecMsg *aud_spc_msg = new AudioSpecMsg(audio_encoder_->get_profile(),
                                                     audio_encoder_->get_channels(),
                                                     audio_encoder_->get_sample_rate());
        aud_spc_msg->pts_ = 0;
        rtmp_pusher->Post(RTMP_BODY_AUD_SPEC, aud_spc_msg);
    }
    // 音频重采样
    auto ret = audio_resampler_->SendResampleFrame(pcm, size);
    if(ret <0)
    {   LogError("SendResampleFrame failed ");
        return;
    }
    //为什么需要一个数组
	//重采样器不保证「每送一次就吐一帧」；
    vector<shared_ptr<AVFrame>> resampled_frames;
    ret = audio_resampler_->ReceiveResampledFrame(
                resampled_frames,
                audio_encoder_->GetFrameSampleSize());
    if(ret !=0)
    {
        LogWarn("ReceiveResampledFrame ret:%d\n",ret);
        return;
    }

    for(int i = 0; i < resampled_frames.size(); i++)
    {
        if(!pcm_flt_fp_)
        {
            pcm_flt_fp_ = fopen("push_dump_flt.pcm", "wb");
        }
        if(pcm_flt_fp_)
        {
            // ffplay -ar 48000 -channels 2 -f f32le  -i push_dump_f32le.pcm
//            fwrite(resampled_frames[i].get()->data[0], 1,
//                    resampled_frames[i].get()->linesize[0], pcm_flt_fp_);
//            fwrite(resampled_frames[i].get()->data[1], 1,
//                    resampled_frames[i].get()->linesize[1], pcm_flt_fp_);
//            fflush(pcm_flt_fp_);
        }
        // 封装带参考计数的缓存
        int aac_size = audio_encoder_->Encode(resampled_frames[i].get(), aac_buf_, AAC_BUF_MAX_LENGTH);
        if(aac_size > 0)
        {
            if(aac_fp_)
            {
                uint8_t adts_header[7];
                audio_encoder_->GetAdtsHeader(adts_header, aac_size);
                fwrite(adts_header, 1, 7, aac_fp_);
                fwrite(aac_buf_, 1, aac_size, aac_fp_);
                fflush(aac_fp_);
            }
            AudioRawMsg *aud_raw_msg = new AudioRawMsg(aac_size + 2);
            // 打上时间戳
            aud_raw_msg->pts = AVPublishTime::GetInstance()->get_audio_pts();
            //构造Audio Tag Data部分
		   /* SoundFormat: 10 (AAC)

			SoundRate : 3 (44kHz，但你可能想要的是 48kHz？RTMP 中它固定是 44kHz，没办法表示 48kHz)

			SoundSize : 1 (16bit)

			SoundType : 1 (Stereo)
            但AAC 音频的 SoundRate 实际是由 AudioSpecificConfig 指定的，而不是 RTMP 头里的 2-bit SoundRate
            */
            aud_raw_msg->data[0] = 0xaf;
            // 根据第一字节的SoundFormat表明要构造AACAduioData
            //然后主动填充AACAudioData的第一个字节AACPacketType的值为1，表明后续字节是AAC Raw Data，可以直接构造RTMPheader去发送
            aud_raw_msg->data[1] = 0x01;   
            memcpy(&aud_raw_msg->data[2], aac_buf_, aac_size);
            rtmp_pusher->Post(RTMP_BODY_AUD_RAW, aud_raw_msg);
            LogDebug("PcmCallback Post");
        }
    }
}

void PushWork::YuvCallback(uint8_t* yuv, int32_t size)
{
    if(video_out_sdl)
        video_out_sdl->Cache(yuv, size);
    char start_code[] = {0, 0, 0, 1};
    if(need_send_video_config)
    {
        need_send_video_config = false;
        VideoSequenceHeaderMsg * vid_config_msg = new VideoSequenceHeaderMsg(
                    video_encoder_->get_sps_data(),
                    video_encoder_->get_sps_size(),
                    video_encoder_->get_pps_data(),
                    video_encoder_->get_pps_size()
                    );
        vid_config_msg->nWidth = video_width_;
        vid_config_msg->nHeight = video_height_;
        vid_config_msg->nFrameRate = video_fps_;
        vid_config_msg->nVideoDataRate = video_bitrate_;
        vid_config_msg->pts_ = 0;
        rtmp_pusher->Post(RTMP_BODY_VID_CONFIG, vid_config_msg);
        if(h264_fp_)
        {

//            fwrite(start_code, 1, 4, h264_fp_);
//            fwrite(video_encoder_->get_sps_data(),
//                   video_encoder_->get_sps_size(), 1, h264_fp_);
//            fwrite(start_code, 1, 4, h264_fp_);
//            fwrite( video_encoder_->get_pps_data(),
//                    video_encoder_->get_pps_size(), 1, h264_fp_);
        }
    }
    // 进行编码，是h264的nalu而不是h265的nalu
    video_nalu_size_ = VIDEO_NALU_BUF_MAX_SIZE;
    if(video_encoder_->Encode(yuv, 0, video_nalu_buf, video_nalu_size_) == 0)
    {
        // 获取到编码数据
        NaluStruct *nalu = new NaluStruct(video_nalu_buf, video_nalu_size_);
        //根据h264结构获取type（nalustruct类构造函数中使用字节四是因为假设我们没有跳过startcode的）
        //0x1f==0x0001 1111 (提取低5位)
        nalu->type = video_nalu_buf[0] & 0x1f;
        nalu->pts = AVPublishTime::GetInstance()->get_video_pts();
        rtmp_pusher->Post(RTMP_BODY_VID_RAW, nalu);
        LogDebug("YuvCallback Post");
//        fwrite(start_code, 1, 4, h264_fp_);
//        fwrite(video_nalu_buf,
//               video_nalu_size_, 1, h264_fp_);
//        fflush(h264_fp_);
    }
}
void PushWork::Loop()
{
    LogInfo("Loop into");
    if(video_out_sdl)
        video_out_sdl->Loop();          // 目前一定要用debug模式
    LogInfo("Loop leave");
}
}

