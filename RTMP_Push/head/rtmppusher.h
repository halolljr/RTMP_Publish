#ifndef RTMPPUSHER_H
#define RTMPPUSHER_H

#include "../librtmp/rtmp.h"
#include "mediabase.h"
#include "rtmpbase.h"
#include "naluloop.h"
#include "dlog.h"
//[完整 RTMP Message]
//│
//▼
//拆分为多个 Chunk 传输
//│
//├─ Chunk 1:
//│    ┌───────────────────────┐
//│    │ Basic Header          │ ← 包含 CSID、Header Type
//│    ├───────────────────────┤
//│    │ Message Header        │ ← 包含时间戳、消息长度、类型、MSID（如有）
//│    ├───────────────────────┤
//│    │ Extended Timestamp    │ ← （可选）
//│    ├───────────────────────┤
//│    │ Chunk Data            │ ← 数据片段
//│    └───────────────────────┘
//│
//├─ Chunk 2:  ...（结构同上）
//│
//└─ Chunk N :
//┌───────────────────────┐
//│ Basic Header          │
//│ Message Header        │
//│ Extended Timestamp    │
//│ Chunk Data            │
//└───────────────────────┘
//│
//▼
//[RTMPPacket]
//- 根据各个 Chunk 中提取的 header 信息：
//- m_nChannel（来自 Basic Header 的 CSID）
//- m_nTimeStamp（Message Header 的时间戳）
//- m_nInfoField2（长消息头中的 MSID）
//- 重组出完整的消息数据（m_body）
namespace LQF
{
enum RTMPPusherMES
{
    RTMPPUSHER_MES_H264_DATA = 1,
    RTMPPUSHER_MES_AAC_DATA = 2
};
typedef struct _RTMPMetadata
{
    // video, must be h264 type
    unsigned int    nWidth;
    unsigned int    nHeight;
    unsigned int    nFrameRate;     // fps
    unsigned int    nVideoDataRate; // bps
    unsigned int    nSpsLen;
    unsigned char   Sps[1024];
    unsigned int    nPpsLen;
    unsigned char   Pps[1024];

    // audio, must be aac type
    bool            bHasAudio;
    unsigned int    audio_sample_rate;     //audiosamplerate
    unsigned int    audio_sample_size;     //audiosamplesize
    unsigned int    nAudioChannels;
    char            pAudioSpecCfg;
    unsigned int    nAudioSpecCfgLen;

} RTMPMetadata, *LPRTMPMetadata;

// 继承了NaluLoop和RTMPBase，实现了异步处理消息
class RTMPPusher : public NaluLoop,public RTMPBase
{
    typedef RTMPBase Super;
public:
    RTMPPusher():RTMPBase(RTMP_BASE_TYPE_PUSH),NaluLoop(30){LogInfo("RTMPPusher create!");}
    //  MetaData
    bool SendMetadata(FLVMetadataMsg *metadata);

    bool SendAudioSpecificConfig(char* data,int length);
private:
    virtual void handle(int what, void *data);
    bool sendH264SequenceHeader(VideoSequenceHeaderMsg *seq_header);
    bool sendH264Packet(char *data,int size, bool is_keyframe, unsigned int timestamp);
    //bool SendAACPacket(char *data,int size, unsigned int timestamp);
    int sendPacket(unsigned int packet_type, unsigned char *data, unsigned int size, unsigned int nTimestamp);

    int64_t time_ = 0; //在SendMetadata获取时间比较准确

    enum
    {
        FLV_CODECID_H264 = 7,
        FLV_CODECID_AAC = 10,
    };

    bool is_first_metadata_ = false;    // 发送metadata
    bool is_first_video_sequence_ = false;
    bool is_first_video_frame_ = false;
    bool is_first_audio_sequence_ = false;
    bool is_first_audio_frame_ = false;
};
}
#endif // RTMPPUSHER_H
