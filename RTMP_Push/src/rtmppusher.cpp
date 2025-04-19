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
//需要构造的就是每一个 RTMP message（也称 FLV tag），该函数从构造Tag Data部分开始
//| -- > RTMP Packet 包括：
//- Type：音频 tag（0x08）、视频 tag（0x09）、meta tag（0x12）
//- Timestamp
//- StreamID
//- Body（也就是 FLV tag 的 Data 部分）
//
//| -- > Body 部分结构：
// Tag Data
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
        //FlV的Audio Tag Data部分的构造
       
        /* SoundFormat: 10 (AAC)

            SoundRate : 3 (44kHz，但你可能想要的是 48kHz？RTMP 中它固定是 44kHz，没办法表示 48kHz)

            SoundSize : 1 (16bit)

            SoundType : 1 (Stereo)
         */
        //Audio Tag Data的第一字节
        aac_spec_[0] = 0xAF;
		//根据第一字节的SoundFormat->构造AACAudioData
		//主动填充AACAudioData的第一个字节AACPacketType是0表明是AAC sequence header
        aac_spec_[1] = 0x0;     
        //根据AACPacketType==0->构造AudioSpecificConfig（格式详见函数开头注释）
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
//AVCDecoderConfigurationRecord 结构
//它是在 AVCPacketType == 0 时跟随的字节序列，用于描述 H.264 编码参数（profile、level、SPS、PPS 等），让解码器知道该如何解析后续的 NALU。
//
//下面是它的结构（按照字节顺序排列）：
//字节索引	字段名	长度	含义
//0	configurationVersion	1 字节	固定值 0x01
//1	AVCProfileIndication	1 字节	SPS[1]，H.264 Profile（如：Baseline, Main, High 等）
//2	profile_compatibility	1 字节	SPS[2]，配置兼容字段
//3	AVCLevelIndication	1 字节	SPS[3]，H.264 Level（如：3.1, 4.0 等）
//4	lengthSizeMinusOne	1 字节	0xFF：后 2 位为 NALU length 字节数 - 1，通常为 0x03 表示 4 字节
//5	numOfSequenceParameterSets	1 字节	高 3 位保留，低 5 位是 SPS 数量，通常是 1，即 0xE1
//6 - 7	SPS length	2 字节	SPS 的长度（大端）
//8 - n	SPS NALU	N 字节	SPS 数据本体
//n + 1	numOfPictureParameterSets	1 字节	PPS 数量，通常是 1
//n + 2~n + 3	PPS length	2 字节	PPS 长度（大端）
//n + 4~n + m	PPS NALU	M 字节	PPS 数据本体
bool RTMPPusher::sendH264SequenceHeader(VideoSequenceHeaderMsg *seq_header)
{
    if (seq_header == NULL)
    {
        return false;
    }
    uint8_t body[1024] = { 0 };
    int i = 0;
    // 构造 Video Tag Data
    
    // 第一个字节
    //0x10（高 4 位）：帧类型，1 表示关键帧（Keyframe）
    //0x07（低 4 位）：编码类型，7 表示 AVC（H.264）
	body[i++] = 0x17; 
    //根据第一个字节的编码类型CodecID==7->构造AVCVideoPacket
    //主动填充AVCVideoPacket的第一个字节AVCPacketTYpe==0,表明是AVC sequence header
    body[i++] = 0x00;
    //3 字节 composition time，这个是 PTS 与 DTS 的差值，用于 B 帧播放偏移。sequence header 设置为 0 即可。
    body[i++] = 0x00;
    body[i++] = 0x00;
    body[i++] = 0x00;
    //根据第一字节的AVCPacketTYpe==0->AVCDecoderConfigurationRecord【结构详见函数注释体】
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
    //为什么11字节？
	/*Video Tag 格式（body） :
	+-------------- - +---------------- + -------------------- + ------------------ - +
		| 1字节 : Frame | 1字节 : AVC Type | 3字节 : Composition | 4字节 : NALU Length |
		| Type + Codec | (0 = seq, 1 = NALU) | Time(CTS offset) | 大端 NALU 长度 |
		+-------------- - +---------------- + -------------------- + ------------------ - +
		| N字节 : H.264 NALU 数据 |
		+---------------------------------------------------------------- +*/
    //对比FLV资料，我们会发现，并没有后续的4字节的NALU Length，这里为什么加上呢？
	//  当 AVCPacketType == 1（即普通 NALU 数据）时，Data 体的结构为：
	/*它不是 Annex B 格式（即不是 startcode 开头），而是：
		多个 NALU，格式为：
		[4字节 NALU 长度][NALU 数据]
		[4字节 NALU 长度][NALU 数据]
		...
		✅ 所以：虽然 FLV 原始规范中没有规定必须写入 NALU size，但...
		RTMP + FLV 中的 H.264 视频数据是以 “长度前缀（length - prefixed NALU）（整个Tag的大小，不仅仅是原始数据大小）” 的格式存放的。
		也就是说，我们自己需要填入每个 NALU 的长度（4 字节），这样播放器才能知道下一个 NALU 的起点在哪里。*/
    //可能有bug，或许改为||更好
    if (data == NULL || size<11)
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

