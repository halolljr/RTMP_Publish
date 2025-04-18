#include "rtmppusher.h"
#include "aacrtmppackager.h"
#include "timeutil.h"
#include "avtimebase.h"
namespace LQF
{
/// <summary>
/// 8位，1字节编成AMF消息
/// </summary>
/// <param name="output">源地址</param>
/// <param name="nVal">AMF消息</param>
/// <returns>output+1</returns>
char * put_byte(char *output, uint8_t nVal)
{
    output[0] = nVal;
    return output + 1;
}
/// <summary>
/// 大端，16位，2字节编成AMF消息
/// </summary>
/// <param name="output">源地址</param>
/// <param name="nVal">AMF消息</param>
/// <returns>output+2</returns>
char * put_be16(char *output, uint16_t nVal)
{
    output[1] = nVal & 0xff;
    output[0] = nVal >> 8;
    return output + 2;
}
char * put_be24(char *output, uint32_t nVal)
{
    output[2] = nVal & 0xff;
    output[1] = nVal >> 8;
    output[0] = nVal >> 16;
    return output + 3;
}
char * put_be32(char *output, uint32_t nVal)
{
    output[3] = nVal & 0xff;
    output[2] = nVal >> 8;
    output[1] = nVal >> 16;
    output[0] = nVal >> 24;
    return output + 4;
}
char *  put_be64(char *output, uint64_t nVal)
{
    output = put_be32(output, nVal >> 32);
    output = put_be32(output, nVal);
    return output;
}
/// <summary>
/// AMF编码，先编码二字节str的长度，后拷贝str长度的数据
/// </summary>
/// <param name="c">原指针</param>
/// <param name="str">源数据</param>
/// <returns>+2+len(str)的原指针</returns>
char * put_amf_string(char *c, const char *str)
{
    uint16_t len = strlen(str);
    //先编长度
    c = put_be16(c, len);
    //拷贝长度的数据数据
    memcpy(c, str, len);
    return c + len;
}
/// <summary>
/// 大端模式，写入一个AMF编码的double值，1+8字节
/// </summary>
/// <param name="c">原指针</param>
/// <param name="d">要写入的double值</param>
/// <returns>返回1+8的指针</returns>
char * put_amf_double(char *c, double d)
{
    // 写入 AMF 类型标识
    *c++ = AMF_NUMBER; 
    {
        unsigned char *ci, *co;
        ci = (unsigned char *)&d;
        co = (unsigned char *)c;
        co[0] = ci[7];
        co[1] = ci[6];
        co[2] = ci[5];
        co[3] = ci[4];
        co[4] = ci[3];
        co[5] = ci[2];
        co[6] = ci[1];
        co[7] = ci[0];
    }
    return c + 8;
}

void RTMPPusher::handle(int what, void *data)
{
    LogDebug("into");
    //要加是否断开连接逻辑
    if(!IsConnect())
    {
        LogInfo("开始断线重连");
        if(!Connect())
        {
            LogInfo("重连失败");
            delete data;
            return;
        }
    }

    switch(what)
    {
    case RTMP_BODY_METADATA:
    {
        if(!is_first_metadata_) {
            is_first_metadata_ = true;
            LogInfo("%s:t%u", AVPublishTime::GetInstance()->getMetadataTag(),
                    AVPublishTime::GetInstance()->getCurrenTime());
        }

        FLVMetadataMsg *metadata = (FLVMetadataMsg*)data;
        if(!SendMetadata(metadata))
        {
            LogError("SendMetadata failed");
        }
        delete metadata;
        break;
    }
    case RTMP_BODY_VID_CONFIG:
    {
        if(!is_first_video_sequence_) {
            is_first_video_sequence_ = true;
            LogInfo("%s:t%u", AVPublishTime::GetInstance()->getAvcHeaderTag(),
                    AVPublishTime::GetInstance()->getCurrenTime());
        }
        VideoSequenceHeaderMsg *vid_cfg_msg = (VideoSequenceHeaderMsg*)data;
        if(!sendH264SequenceHeader(vid_cfg_msg))
        {
            LogError("sendH264SequenceHeader failed");
        }
        delete vid_cfg_msg;
        break;
    }
    case RTMP_BODY_VID_RAW:
    {
        if(!is_first_video_frame_) {
            is_first_video_frame_ = true;
            LogInfo("%s:t%u", AVPublishTime::GetInstance()->getAvcFrameTag(),
                    AVPublishTime::GetInstance()->getCurrenTime());
        }

        NaluStruct* nalu = (NaluStruct*)data;
        if(sendH264Packet((char*)nalu->data,nalu->size,(nalu->type == 0x05) ? true : false,
                          nalu->pts))
        {
            //LogInfo("send pack ok");
        }
        else
        {
            LogInfo("at handle send h264 pack fail");
        }
        delete nalu;                       //注意要用new 会调用析构函数，释放内部空间
        break;
    }
    case RTMP_BODY_AUD_SPEC:
    {
        if(!is_first_audio_sequence_) {
            is_first_audio_sequence_ = true;
            LogInfo("%s:t%u", AVPublishTime::GetInstance()->getAacHeaderTag(),
                    AVPublishTime::GetInstance()->getCurrenTime());
        }
        AudioSpecMsg* audio_spec = (AudioSpecMsg*)data;
        uint8_t aac_spec_[4];
        /* SoundFormat: 10 (AAC)

            SoundRate : 3 (44kHz，但你可能想要的是 48kHz？RTMP 中它固定是 44kHz，没办法表示 48kHz)

            SoundSize : 1 (16bit)

            SoundType : 1 (Stereo)
         */
        aac_spec_[0] = 0xAF;
        // 0 = aac sequence header
        aac_spec_[1] = 0x0;     
        //因为0x00 表示这是一个 AAC sequence header，所以要生成配置信息
        AACRTMPPackager::GetAudioSpecificConfig(&aac_spec_[2], audio_spec->profile_,
                audio_spec->sample_rate_, audio_spec->channels_);
        SendAudioSpecificConfig((char *)aac_spec_, 4);
        break;
    }
    case RTMP_BODY_AUD_RAW:
    {
        if(!is_first_audio_frame_) {
            is_first_audio_frame_ = true;
            LogInfo("%s:t%u", AVPublishTime::GetInstance()->getAacDataTag(),
                    AVPublishTime::GetInstance()->getCurrenTime());
        }
        AudioRawMsg* audio_raw = (AudioRawMsg*)data;
        if(sendPacket(RTMP_PACKET_TYPE_AUDIO, (unsigned char*)audio_raw->data,
                      audio_raw->size, audio_raw->pts))
        {

        }
        else
        {
            LogInfo("at handle send audio pack fail");
        }
        delete audio_raw;                       //注意要用new 会调用析构函数，释放内部空间
        break;
    }
    default:
        break;
    }
    LogDebug("leave");
}

/// <summary>
/// 向 RTMP 服务器发送 FLV Metadata（onMetaData）信息包，这通常是 RTMP 推流过程的第一步，用于告诉服务器（和之后的播放端）
/// </summary>
/// <param name="metadata"></param>
/// <returns></returns>
bool RTMPPusher::SendMetadata(FLVMetadataMsg *metadata)
{
    if (metadata == NULL)
    {
        return false;
    }
    char body[1024] = { 0 };

    char * p = (char *)body;
    //AMF编码：键值对的类型，先长度后数据
    //写入方法名 @setDataFrame（固定写法）
    p = put_byte(p, AMF_STRING);    
    p = put_amf_string(p, "@setDataFrame");
    //写入事件名 "onMetaData"（固定）
    p = put_byte(p, AMF_STRING);
    p = put_amf_string(p, "onMetaData");
    //写入 Metadata 键值对（AMF_OBJECT）
    p = put_byte(p, AMF_OBJECT);
    p = put_amf_string(p, "copyright");
    p = put_byte(p, AMF_STRING);
    p = put_amf_string(p, "firehood");

    if(metadata->has_video)
    {
        p = put_amf_string(p, "width");
        p = put_amf_double(p, metadata->width);

        p = put_amf_string(p, "height");
        p = put_amf_double(p, metadata->height);

        p = put_amf_string(p, "framerate");
        p = put_amf_double(p, metadata->framerate);

        p = put_amf_string(p, "videodatarate");
        p = put_amf_double(p, metadata->videodatarate);

        p = put_amf_string(p, "videocodecid");
        p = put_amf_double(p, FLV_CODECID_H264);
    }
    if(metadata->has_audio)
    {
        p = put_amf_string(p, "audiodatarate");
        p = put_amf_double(p, (double)metadata->audiodatarate);

        p = put_amf_string(p, "audiosamplerate");
        p = put_amf_double(p, (double)metadata->audiosamplerate);

        p = put_amf_string(p, "audiosamplesize");
        p = put_amf_double(p, (double)metadata->audiosamplesize);

        p = put_amf_string(p, "stereo");
        p = put_amf_double(p, (double)metadata->channles);

        p = put_amf_string(p, "audiocodecid");
        p = put_amf_double(p, (double)FLV_CODECID_AAC);
    }
    //写 AMF_OBJECT 结尾标志
    p = put_amf_string(p, "");
    p = put_byte(p, AMF_OBJECT_END);
    
    return sendPacket(RTMP_PACKET_TYPE_INFO, (unsigned char*)body, p - body, 0);
}

/// <summary>
/// 将 H.264 编码的视频 SPS/PPS 封装成 FLV 中的 AVCDecoderConfigurationRecord 格式，并发送给 RTMP 服务器。
/// </summary>
/// <param name="seq_header"></param>
/// <returns></returns>
bool RTMPPusher::sendH264SequenceHeader(VideoSequenceHeaderMsg *seq_header)
{
    if (seq_header == NULL)
    {
        return false;
    }
    uint8_t body[1024] = { 0 };
    int i = 0;
    // 构造 FLV 视频包头部（5字节）
    //0x10（高 4 位）：帧类型，1 表示关键帧（Keyframe）
    //0x07（低 4 位）：编码类型，7 表示 AVC（H.264）
	body[i++] = 0x17; 
    //0x00: 表示是 AVC sequence header（而不是 NALU 数据）
    body[i++] = 0x00;
    //3 字节 composition time，这个是 PTS 与 DTS 的差值，用于 B 帧播放偏移。sequence header 设置为 0 即可。
    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;

    // 写入 AVCDecoderConfigurationRecord
    body[i++] = 0x01;               // configurationVersion
    body[i++] = seq_header->sps_[1]; // AVCProfileIndication
    body[i++] = seq_header->sps_[2]; // profile_compatibility
    body[i++] = seq_header->sps_[3]; // AVCLevelIndication
    body[i++] = 0xff;               // lengthSizeMinusOne（NALU 前缀长度-1，表示 4 字节前缀）

    // 写入 SPS（序列参数集）
    body[i++] = 0xE1;                 //表示有1个 SPS（&0x1f = 1）
    // sps data length
    body[i++] = (seq_header->sps_size_ >> 8) & 0xff;;
    body[i++] = seq_header->sps_size_ & 0xff;
    // sps data
    memcpy(&body[i], seq_header->sps_, seq_header->sps_size_);
    i = i + seq_header->sps_size_;

    // 写入 PPS（图像参数集）
    body[i++] = 0x01; // 表示有1个 PPS
    // pps data length
    body[i++] = (seq_header->pps_size_ >> 8) & 0xff;;
    body[i++] = seq_header->pps_size_ & 0xff;
    // sps data
    memcpy(&body[i], seq_header->pps_, seq_header->pps_size_);
    i = i + seq_header->pps_size_;

    time_ = TimesUtil::GetTimeMillisecond();
    return sendPacket(RTMP_PACKET_TYPE_VIDEO, (unsigned char*)body, i, 0);
}

bool RTMPPusher::SendAudioSpecificConfig(char* data,int length)
{
    if(data == NULL)
    {
        return false;
    }
    RTMPPacket packet;
    RTMPPacket_Reset(&packet);
    RTMPPacket_Alloc(&packet, 4);

    packet.m_body[0] = data[0]; // 0xAF
    packet.m_body[1] = data[1]; // 0x00
    packet.m_body[2] = data[2]; // AAC config byte 1
    packet.m_body[3] = data[3]; // AAC config byte 2

    packet.m_headerType  = RTMP_PACKET_SIZE_LARGE;
    packet.m_packetType = RTMP_PACKET_TYPE_AUDIO;
    packet.m_nChannel   = RTMP_AUDIO_CHANNEL;
    packet.m_nTimeStamp = 0;
    packet.m_nInfoField2 = rtmp_->m_stream_id;
    packet.m_nBodySize  = 4;

    //调用发送接口 发送一个消息
    int nRet = RTMP_SendPacket(rtmp_, &packet, 0);
    if (nRet != 1)
    {
        LogInfo("RTMP_SendPacket fail %d\n",nRet);
    }
    RTMPPacket_Free(&packet);//释放内存
    return (nRet = 0?true:false);
}

bool RTMPPusher::sendH264Packet(char *data,int size, bool is_keyframe, unsigned int timestamp)
{
    if (data == NULL && size<11)
    {
        return false;
    }

    unsigned char *body = new unsigned char[size + 9];

    int i = 0;
    if (is_keyframe)
    {
        body[i++] = 0x17;// 1:Iframe  7:AVC
    }
    else
    {
        body[i++] = 0x27;// 2:Pframe  7:AVC
    }
    body[i++] = 0x01;// AVC NALU
    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;

    // NALU size
    body[i++] = size >> 24;
    body[i++] = size >> 16;
    body[i++] = size >> 8;
    body[i++] = size & 0xff;;

    // NALU data
    memcpy(&body[i], data, size);

    bool bRet = sendPacket(RTMP_PACKET_TYPE_VIDEO, body, i + size, timestamp);
    delete[] body;
    return bRet;
}
//📦 RTMP 与 FLV 的映射关系简图：
//RTMP 本身不是 FLV 文件，而是 FLV 的“封装形式”，它只承载 Tag Data 部分，并通过 RTMP 协议头部（channel、packet type、timestamp 等）来表达原来 FLV Tag Header 的信息
//FLV文件结构	RTMP推流结构
//Tag Header	→ 映射为 RTMP Header（由 librtmp 构造），具体体现在sendPacket函数中和 RTMP_SendPacket函数中
//Tag Data	→ 调用sendPacket的函数(AudioSpecificConfig就更详细了)
//PreviousTagSize	→ RTMP 不需要（FLV 文件结构中才用）
int RTMPPusher::sendPacket(unsigned int packet_type, unsigned char *data,
                           unsigned int size, unsigned int timestamp)
{
    if (rtmp_ == NULL)
    {
        return FALSE;
    }

    //构造RTMP包
    RTMPPacket packet;
    RTMPPacket_Reset(&packet);
    RTMPPacket_Alloc(&packet, size);

    packet.m_packetType = packet_type;  
    if(packet_type == RTMP_PACKET_TYPE_AUDIO)
    {
        packet.m_nChannel = RTMP_AUDIO_CHANNEL;
        //               LogInfo("audio packet timestamp:%u", timestamp);
    }
    else if(packet_type == RTMP_PACKET_TYPE_VIDEO)
    {
        packet.m_nChannel = RTMP_VIDEO_CHANNEL;
//                      LogInfo("video packet timestamp:%u, size:%u", timestamp, size);
    }
    else
    {
        packet.m_nChannel = RTMP_NETWORK_CHANNEL;   // 0x03，控制信息通道
    }
    packet.m_headerType = RTMP_PACKET_SIZE_LARGE;   // metadata 是 large type（有完整时间戳）
    packet.m_nTimeStamp = timestamp;    // 一般为 0
    packet.m_nInfoField2 = rtmp_->m_stream_id;  // 绑定的 RTMP stream id
    packet.m_nBodySize = size;
    memcpy(packet.m_body, data, size);

    int nRet = RTMP_SendPacket(rtmp_, &packet, 0);
    if (nRet != 1)
    {
        LogInfo("RTMP_SendPacket fail %d\n",nRet);
    }

    RTMPPacket_Free(&packet);

    return nRet;
}
}

