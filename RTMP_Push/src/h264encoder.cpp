﻿#include "h264encoder.h"
#include "dlog.h"
namespace LQF
{


H264Encoder::H264Encoder()
{

}

/**
 * @brief 编码codec 的time_base实际上表示视频的帧率，
 * qmin,qmax决定了编码的质量，qmin，qmax越大编码的质量越差。
 * zerolatency参数的作用是提搞编码的实时性。
 * 更详细的设置参考:
 * https://trac.ffmpeg.org/wiki/Encode/H.264
 * ffmpeg libx264 h264_nvenc 编码参数解析 https://blog.csdn.net/leiflyy/article/details/87935084
 * @param properties
 * @return
 */
int H264Encoder::Init(const Properties &properties)
{
    //宽高必须为偶数，因为 YUV420 格式中，U/V 分量是 2x2 的 block，要求宽高都为偶数
    width_ = properties.GetProperty("width", 0);
    if(width_ == 0 || width_%2 != 0)
    {
        LogError("width:%d", width_);
        return RET_ERR_NOT_SUPPORT;
    }
    height_ = properties.GetProperty("height", 0);
    if(height_ == 0 || height_%2 != 0)
    {
        LogError("height:%d", height_);
        return RET_ERR_NOT_SUPPORT;
    }
    fps_ = properties.GetProperty("fps", 25);
    b_frames_ = properties.GetProperty("b_frames", 0);
    bitrate_ = properties.GetProperty("bitrate", 500*1024);
    gop_ = properties.GetProperty("gop", fps_);
    //寻找编码器
    codec_ = avcodec_find_encoder(AV_CODEC_ID_H264);
    if (!codec_)
    {
        printf("Can not find encoder! \n");

    }

    count = 0;
    framecnt = 0;

    ctx_ = avcodec_alloc_context3(codec_);

    //Param that must set

    //最大和最小量化系数，取值范围为0~51。越小质量越高，但压缩效率低。
    ctx_->qmin = 10;
    ctx_->qmax = 31;

    //编码后的视频帧大小，以像素为单位。
    ctx_->width = width_;
    ctx_->height = height_;

    //编码后的码率：值越大越清晰，值越小越流畅。
    ctx_->bit_rate = bitrate_;

    //每20帧插入一个I帧
    ctx_->gop_size = gop_;

    //帧率的基本单位，time_base.num为时间线分子，time_base.den为时间线分母，帧率=分子/分母。
    ctx_->time_base.num = 1;
    ctx_->time_base.den = fps_;

    ctx_->framerate.num = fps_;
    ctx_->framerate.den = 1;

    //图像色彩空间的格式，采用什么样的色彩空间来表明一个像素点。
    ctx_->pix_fmt = AV_PIX_FMT_YUV420P;

    //编码器编码的数据类型
    ctx_->codec_type = AVMEDIA_TYPE_VIDEO;

    //Optional Param
    //两个非B帧之间允许出现多少个B帧数，设置0表示不使用B帧，没有编码延时。B帧越多，压缩率越高。
    ctx_->max_b_frames = b_frames_;

    if (ctx_->codec_id == AV_CODEC_ID_H264) {
        av_dict_set(&param, "preset", "ultrafast", 0);  //"preset": "ultrafast"：编码效率优先，压缩率差，但快。
        av_dict_set(&param, "tune", "zerolatency", 0);  //关闭 B 帧、去除延迟缓存，非常适合 实时推流 / 实时通信。
    }
    if (ctx_->codec_id == AV_CODEC_ID_H265){
        av_dict_set(&param, "preset", "ultrafast", 0);
        av_dict_set(&param, "tune", "zero-latency", 0);
    }

    //表示使用全局头信息，如 SPS / PPS 放在 extradata 中，而不是每个关键帧前。
    ctx_->flags |= AV_CODEC_FLAG_GLOBAL_HEADER;    
	//初始化并打开编码器，当你调用 avcodec_open2() 启用编码器时，FFmpeg 的编码器（比如 libx264）就会生成 SPS 和 PPS，并存在 AVCodecContext::extradata 里。
    if (avcodec_open2(ctx_, codec_, &param) < 0)
    {
        printf("Failed to open encoder! \n");
    }
	/*🔹 SPS（Sequence Parameter Set）
		表示序列参数集，描述了一整个视频序列的编码参数，包括：

		视频分辨率（宽高）

		颜色格式（比如 YUV）

		编码级别（profile、level）

		帧率相关信息

		最大参考帧数量等

		👉 播放器必须先拿到 SPS才能正确初始化解码器，否则就不知道你的视频是什么规格。

		🔸 PPS（Picture Parameter Set）
		表示图像参数集，描述单帧或多帧图像的解码参数，包括：

		熵编码模式（CABAC / CAVLC）

		参考帧索引方式

		宏块信息

		播放器先用 SPS 构建一个解码器，再根据 PPS 去解码每一帧视频。*/
    // 读取sps pps 信息
    if(ctx_->extradata)
    {
        LogWarn("extradata_size:%d", ctx_->extradata_size);
        // 第一个为sps 7
        // 第二个为pps 8
        
        //默认开头是 0x00 00 00 01（4字节 start code），跳过之后是SPS
        uint8_t *sps = ctx_->extradata + 4;    // 直接跳到数据
        int sps_len = 0;
        uint8_t *pps = NULL;
        int pps_len = 0;

        uint8_t *data = ctx_->extradata + 4;

        //然后往后找下一个 start code（表示 PPS 起始）
        //为什么减去四字节？因为防止下面的遍历越界
        for (int i = 0; i < ctx_->extradata_size - 4; ++i)
        {
            if (0 == data[i] && 0 == data[i + 1] && 0 == data[i + 2] && 1 == data[i + 3])
            {
                pps = &data[i+4];
                break;
            }
        }
        //计算并填充SPS和PPS
        sps_len = int(pps - sps) - 4;   // 4是00 00 00 01占用的字节
        pps_len = ctx_->extradata_size - 4*2 - sps_len;
        sps_.append(sps, sps + sps_len);
        pps_.append(pps, pps + pps_len);
    }

    //重点，建议搞清楚内存图！！！！！！！！！
    //Init frame
    frame_ = av_frame_alloc();
    //计算一帧图像的字节数【详解见yuv420P的字节大小计算方式】
    int pictureSize = avpicture_get_size(ctx_->pix_fmt, ctx_->width, ctx_->height);
    picture_buf_ = (uint8_t *)av_malloc(pictureSize);
    //将缓冲区与 AVFrame 绑定
    // 因为是planar格式
    //这样之后你只需要往 frame_->data[0]、data[1]、data[2] 里填像素数据即可，不需要手动处理 offset。
    //特比注明：frame->linesize代表对应的yuv分量的总行字节
    //推荐使用更新的av_frame_get_buffer和av_frame_fill，以实现内存对齐
    avpicture_fill((AVPicture *)frame_, picture_buf_, ctx_->pix_fmt, ctx_->width, ctx_->height);

    frame_->width = ctx_->width;
    frame_->height = ctx_->height;
    frame_->format = ctx_->pix_fmt;

    //Init packet
    av_new_packet(&packet_, pictureSize);
    data_size_ = ctx_->width * ctx_->height;

    return 0;
}

H264Encoder::~H264Encoder()
{
    //销毁
    if(ctx_)
        avcodec_close(ctx_);
    if(frame_)
        av_free(frame_);
    if(picture_buf_)
        av_free(picture_buf_);
    av_free_packet(&packet_);
}

int H264Encoder::Encode(uint8_t *in, int in_samples, uint8_t *out, int &out_size)
{
    //data_size=width*height;
    frame_->data[0] = in;                         // Y的起始数据地址
    frame_->data[1] = in + data_size_;              // U的其实数据地址
    frame_->data[2] = in + data_size_ * 5 / 4;      // V的起始数据地址（in+data_size+data_size/4）
    //时间戳,0、1、2...，做音视频同步的时候会进行一个转化：pts*av_q2d(time_base)
    frame_->pts = (count++)*(ctx_->time_base.den) / ((ctx_->time_base.num) * 24);  

    av_init_packet(&packet_);
    //Encode，成功后 packet_.data 就是 一帧 H.264 视频帧的裸流数据（NALU）。
	//编码成功但不够数据的时候,got_ouput会为0，此时还需要送帧
	//建议使用新版本的
    //想想新版本要怎么做！！！！！！！！！
    int got_picture = 0;
    int ret = avcodec_encode_video2(ctx_, &packet_, frame_, &got_picture);

    if (ret < 0)
    {
        printf("Failed to encode! \n");
        return -1;
    }

    if (got_picture == 1)
    {
//        printf("Succeed to encode frame: %5d\tsize:%5d\n", framecnt, packet.size);
        framecnt++;
        //如果你没有特别设置 AVCodecContext 的 flags 或 bitstream_filter，FFmpeg 默认就是输出 4 字节 start code。
        // 跳过00 00 00 01 startcode nalu，拷贝packet_.size - 4个字节就是跳过00 00 00 01的裸流
        memcpy(out, packet_.data + 4, packet_.size - 4);
        out_size = packet_.size - 4;
        av_packet_unref(&packet_);       //释放内存 不释放则内存泄漏
        return 0;
    }

    return -1;
}

int H264Encoder::Encode(AVFrame *frame, uint8_t *out, int &out_size)
{
    return 0;
}

int H264Encoder::get_sps(uint8_t *sps, int &sps_len)
{
    if(!sps || sps_len < sps_.size())
    {
        LogError("sps or sps_len failed");
        return -1;
    }
    sps_len = sps_.size();
    memcpy(sps, sps_.c_str(), sps_len);
    return 0;
}

int H264Encoder::get_pps(uint8_t *pps, int &pps_len)
{
    if(!pps || pps_len < pps_.size())
    {
        LogError("pps or pps_len failed");
        return -1;
    }
    pps_len = pps_.size();
    memcpy(pps, pps_.c_str(), pps_len);
    return 0;
}

AVPacket *H264Encoder::Encode(uint8_t *yuv, uint64_t pts, const int flush)
{
    frame_->data[0] = yuv;                         // Y
    frame_->data[1] = yuv + data_size_;              // U
    frame_->data[2] = yuv + data_size_ * 5 / 4;      // V


    if (pts != 0) {
        frame_->pts = pts;
    }
    else {
        frame_->pts = pts_++;
    }
    AVRational src_time_base = AVRational{1, 1000};
    //    frame_->pts =av_rescale_q(frame_->pts, src_time_base, ctx_->time_base);
    //            (frame_->pts)*(ctx_->time_base.den) / ((ctx_->time_base.num) * 25);  //时间戳


    frame_->pict_type = AV_PICTURE_TYPE_NONE;
    //    if (force_idr_) {
    //        frame_->pict_type = AV_PICTURE_TYPE_I;
    //        force_idr_ = false;
    //    }

    if (avcodec_send_frame(ctx_, frame_) < 0) {
        LogError("avcodec_send_frame() failed.\n");
        return NULL;
    }

    AVPacket *packet = av_packet_alloc();

    int ret = avcodec_receive_packet(ctx_, packet);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF) {
        return NULL;
    }
    else if (ret < 0) {
        LogError("avcodec_receive_packet() failed.");
        return NULL;
    }
//    LogInfo("vpts1:%ld", frame_->pts);
//    LogInfo("vpts2:%ld", packet->pts);
    return packet;
}
}
