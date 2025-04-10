#ifndef __RTMP_H__
#define __RTMP_H__
/*
 *      Copyright (C) 2005-2008 Team XBMC
 *      http://www.xbmc.org
 *      Copyright (C) 2008-2009 Andrej Stepanchuk
 *      Copyright (C) 2009-2010 Howard Chu
 *
 *  This file is part of librtmp.
 *
 *  librtmp is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU Lesser General Public License as
 *  published by the Free Software Foundation; either version 2.1,
 *  or (at your option) any later version.
 *
 *  librtmp is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License
 *  along with librtmp see the file COPYING.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA  02110-1301, USA.
 *  http://www.gnu.org/copyleft/lgpl.html
 */

#if !defined(NO_CRYPTO) && !defined(CRYPTO)
#define CRYPTO
#endif

#include <errno.h>
#include <stdint.h>
#include <stddef.h>

#include "amf.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define RTMP_LIB_VERSION	0x020300	/* 2.3 */

#define RTMP_FEATURE_HTTP	0x01
#define RTMP_FEATURE_ENC	0x02
#define RTMP_FEATURE_SSL	0x04
#define RTMP_FEATURE_MFP	0x08	/* not yet supported */
#define RTMP_FEATURE_WRITE	0x10	/* publish, not play */
#define RTMP_FEATURE_HTTP2	0x20	/* server-side rtmpt */

#define RTMP_PROTOCOL_UNDEFINED	-1
#define RTMP_PROTOCOL_RTMP      0
#define RTMP_PROTOCOL_RTMPE     RTMP_FEATURE_ENC
#define RTMP_PROTOCOL_RTMPT     RTMP_FEATURE_HTTP
#define RTMP_PROTOCOL_RTMPS     RTMP_FEATURE_SSL
#define RTMP_PROTOCOL_RTMPTE    (RTMP_FEATURE_HTTP|RTMP_FEATURE_ENC)
#define RTMP_PROTOCOL_RTMPTS    (RTMP_FEATURE_HTTP|RTMP_FEATURE_SSL)
#define RTMP_PROTOCOL_RTMFP     RTMP_FEATURE_MFP

#define RTMP_DEFAULT_CHUNKSIZE	128

/* needs to fit largest number of bytes recv() may return */
#define RTMP_BUFFER_CACHE_SIZE (16*1024)

#define	RTMP_CHANNELS	65600

  extern const char RTMPProtocolStringsLower[][7];
  extern const AVal RTMP_DefaultFlashVer;
  extern int RTMP_ctrlC;

  uint32_t RTMP_GetTime(void);

/*      RTMP_PACKET_TYPE_...                0x00 */
#define RTMP_PACKET_TYPE_CHUNK_SIZE         0x01
/*      RTMP_PACKET_TYPE_...                0x02 */
#define RTMP_PACKET_TYPE_BYTES_READ_REPORT  0x03
#define RTMP_PACKET_TYPE_CONTROL            0x04
#define RTMP_PACKET_TYPE_SERVER_BW          0x05
#define RTMP_PACKET_TYPE_CLIENT_BW          0x06
/*      RTMP_PACKET_TYPE_...                0x07 */
#define RTMP_PACKET_TYPE_AUDIO              0x08
#define RTMP_PACKET_TYPE_VIDEO              0x09
/*      RTMP_PACKET_TYPE_...                0x0A */
/*      RTMP_PACKET_TYPE_...                0x0B */
/*      RTMP_PACKET_TYPE_...                0x0C */
/*      RTMP_PACKET_TYPE_...                0x0D */
/*      RTMP_PACKET_TYPE_...                0x0E */
#define RTMP_PACKET_TYPE_FLEX_STREAM_SEND   0x0F
#define RTMP_PACKET_TYPE_FLEX_SHARED_OBJECT 0x10
#define RTMP_PACKET_TYPE_FLEX_MESSAGE       0x11
#define RTMP_PACKET_TYPE_INFO               0x12
#define RTMP_PACKET_TYPE_SHARED_OBJECT      0x13
#define RTMP_PACKET_TYPE_INVOKE             0x14
/*      RTMP_PACKET_TYPE_...                0x15 */
#define RTMP_PACKET_TYPE_FLASH_VIDEO        0x16

#define RTMP_MAX_HEADER_SIZE 18 //basic header最大为3字节，message header最带为11字节，加上extended header为4字节

#define RTMP_PACKET_SIZE_LARGE    0
#define RTMP_PACKET_SIZE_MEDIUM   1
#define RTMP_PACKET_SIZE_SMALL    2
#define RTMP_PACKET_SIZE_MINIMUM  3

  // 调用者想获取raw chunk数据自己组成message的时候才用得上,描述整个chunk header的信息
  typedef struct RTMPChunk
  {
    int c_headerSize;   //表示整个chunk header的大小(Basic Header + Message Header + Extended Timestamp的总和)​，单位为字节。RTMP 协议中，块头部的长度可能为 12、8、4 或 1 字节[这里是假定basic header为1字节]，具体取决于块类型和其他因素.
    int c_chunkSize;    //表示chunk data的大小
    char *c_chunk;  //指向chunk data部分。该指针指向实际的块数据内容，存储着需要传输的音视频数据或其他信息。
    //一个字符数组，实际上包含了Basic Header + Message Header + Extended Timestamp，用于存储块的头部信息。数组的大小由宏 RTMP_MAX_HEADER_SIZE 定义，通常设置为能够容纳最大块头部长度的值。
    char c_header[RTMP_MAX_HEADER_SIZE];    
  } RTMPChunk;

  // 这个结构体实际上就是对应RTMP协议中的Message
  typedef struct RTMPPacket
  {
    uint8_t m_headerType;   //chunk msg header的类型(0-3,对应11/7/3/0字节)
    uint8_t m_packetType;   //Message type ID（1-7协议控制；8，9音视频；10以后为AMF编码消息）
    uint8_t m_hasAbsTimestamp;	// Timestamp 是绝对值还是相对值
    int m_nChannel;         //块流ID(chunk stream id, 音视频有独立的csid)
    uint32_t m_nTimeStamp;	//  时间戳
    int32_t m_nInfoField2;	// msg stream id,消息流ID
    uint32_t m_nBodySize;   // 原始数据的长度
    uint32_t m_nBytesRead;  // 已读message长度
    RTMPChunk *m_chunk;     // 执行raw chunk数据
    char *m_body;           // message数据buffer
  } RTMPPacket;


  //RTMPSockBuf 结构体用于管理与 RTMP 连接相关的底层 socket 缓冲区，主要用于存储从 socket 中读取的数据，并跟踪缓冲区内未处理的数据状态。
  typedef struct RTMPSockBuf
  {
    int sb_socket;  //保存 socket 的文件描述符或句柄，用于网络通信的标识。
    int sb_size;		//记录当前缓冲区中未处理的字节数。
    char *sb_start;		//指向 sb_buf 缓冲区中下一个待处理字节的位置。
    char sb_buf[RTMP_BUFFER_CACHE_SIZE];	//作为从 socket 中读取数据的临时缓存区，存储原始的网络数据，供后续解析和处理使用。
    int sb_timedout;    //如果网络读取操作超时，该字段会被设置，用于后续错误处理或重试逻辑。
    void *sb_ssl;   //指向 SSL/TLS 相关数据的指针（例如 SSL 结构体），如果使用加密连接，则会被初始化。
  } RTMPSockBuf;

  void RTMPPacket_Reset(RTMPPacket *p);
  void RTMPPacket_Dump(RTMPPacket *p);
  int RTMPPacket_Alloc(RTMPPacket *p, uint32_t nSize);
  void RTMPPacket_Free(RTMPPacket *p);

#define RTMPPacket_IsReady(a)	((a)->m_nBytesRead == (a)->m_nBodySize)

  //这个结构体主要用于存储建立 RTMP 连接时所需的各种参数和选项，包含 URL 解析后的各个部分以及连接、认证、加密、SWF 验证等相关信息。
  typedef struct RTMP_LNK
  {
    AVal hostname;  //存储 RTMP 服务器的主机名。
    AVal sockshost; //如果使用 SOCKS 代理，这里存储代理服务器的主机名。

	AVal playpath0;	//从 RTMP URL 中解析出的原始播放路径，通常表示流的标识名称。例如 URL 中 “ / live / stream” 解析出 “stream” 部分。
    AVal playpath;	//显式传入的播放路径，可以覆盖 URL 解析出的 playpath0，常用于对播放路径进行手动调整。
    AVal tcUrl; //target connection URL，即用于建立连接的 URL。通常包含协议、主机名、端口和应用名部分。
    AVal swfUrl;    //SWF 文件的 URL，用于 SWF 验证（防盗链机制），验证客户端是否有权播放流。
    AVal pageUrl;   //页面 URL，通常用作 referer，有助于服务端判断请求的合法性。
    AVal app;   //应用名称，在 RTMP 服务器上标识具体的应用（或虚拟目录），例如 “live” 或 “vod”。
    AVal auth;  //认证参数，用于流访问控制。可附加在 URL 中或作为独立参数传递。
    AVal flashVer;  //表示客户端使用的 Flash 版本字符串，服务器可能基于此信息决定是否允许连接或选择特定的传输方式。
    AVal subscribepath; //订阅路径，用于某些直播或多流场景，指定订阅的特定子流或频道。
    AVal usherToken;    //辅助认证或访问控制使用的令牌，可能由服务器下发，用于后续验证。
    AVal token; //另外一个认证令牌，与 auth 参数类似，用于加强访问安全性。
    //用于发布（推流）时的用户名和密码，确保只有经过认证的用户才能发布内容。
    AVal pubUser;
    AVal pubPasswd;
    //额外的参数（以 AMF 格式存储），edepth 记录 extras 的深度或参数个数，便于传递复杂的用户自定义参数。
    AMFObject extras;
    int edepth;

    int seekTime;   //设置播放时的起始位置（秒），用于从特定时间点开始播放
    int stopTime;   //设置播放结束时间，用于截取流的一部分进行播放。

#define RTMP_LF_AUTH	0x0001	/* using auth param */
#define RTMP_LF_LIVE	0x0002	/* stream is live */
#define RTMP_LF_SWFV	0x0004	/* do SWF verification */
#define RTMP_LF_PLST	0x0008	/* send playlist before play */
#define RTMP_LF_BUFX	0x0010	/* toggle stream on BufferEmpty msg */
#define RTMP_LF_FTCU	0x0020	/* free tcUrl on close */
#define RTMP_LF_FAPU	0x0040	/* free app on close */
	int lFlags; //这是一个标志位（位掩码），每个位表示一个连接选项，常见的标志有：

    int swfAge; //用于 SWF 验证时的 SWF 文件缓存时间或过期时间（单位可能为秒），帮助判断 SWF 是否需要重新验证。

    int protocol;   //指定所使用的传输协议，如 RTMP、RTMPT、RTMPS 等，决定了连接的底层传输方式。
    int timeout;		//连接超时时间（以秒为单位），在建立连接时用于超时控制。

    int pFlags;			//保留字段，当前未使用，但为了保持 ABI 的兼容性而保留。

    unsigned short socksport;   //SOCKS 代理的端口号（如果使用 SOCKS 代理时有效）。
    unsigned short port;    //连接 RTMP 服务器所用的端口号（默认通常为 1935）。

#ifdef CRYPTO
#define RTMP_SWF_HASHLEN	32
    void *dh;			/* for encryption */
    void *rc4keyIn;
    void *rc4keyOut;

    uint32_t SWFSize;
    uint8_t SWFHash[RTMP_SWF_HASHLEN];
    char SWFVerificationResponse[RTMP_SWF_HASHLEN+10];
#endif
  } RTMP_LNK;

  /* state for read() wrapper */
  typedef struct RTMP_READ
  {
    char *buf;  //指向数据缓冲区的起始地址，存储从网络或其他数据源读取到的数据。
    char *bufpos;   //指向当前读取位置的指针，用于在缓冲区中跟踪已处理与未处理的数据。
    unsigned int buflen;    //表示缓冲区中数据的总长度（字节数），帮助判断数据是否读完或者还需要继续读取。
    uint32_t timestamp; //当前 chunk 或消息的时间戳，用于同步音视频数据及其它时间相关处理。
    uint8_t dataType;   //指示当前数据的类型（例如音频、视频或控制消息），有助于后续数据解析与处理。
    uint8_t flags;  //用于标记读取过程中的各种状态。如下:
#define RTMP_READ_HEADER	0x01
#define RTMP_READ_RESUME	0x02
#define RTMP_READ_NO_IGNORE	0x04
#define RTMP_READ_GOTKF		0x08
#define RTMP_READ_GOTFLVK	0x10
#define RTMP_READ_SEEKING	0x20
    int8_t status;  //一个有符号整型变量，用于描述当前读取操作的状态。预定义了几种状态：
#define RTMP_READ_COMPLETE	-3
#define RTMP_READ_ERROR	-2
#define RTMP_READ_EOF	-1
#define RTMP_READ_IGNORE	0

    /* 针对断点续传的字段（当 bResume 为 TRUE 时启用） */
    uint8_t initialFrameType;   //记录初始帧的类型，用于标识断点续传时接收到的第一个帧。
    uint32_t nResumeTS; //续传时的时间戳，标识续传起点。
    char *metaHeader;   //存储元数据头信息及其长度，这通常与 FLV 数据格式相关。
    char *initialFrame; //存储初始帧数据及其长度，用于续传时的数据拼接。
    uint32_t nMetaHeaderSize;   //存储元数据头信息及其长度，这通常与 FLV 数据格式相关。
    uint32_t nInitialFrameSize; //存储初始帧数据及其长度，用于续传时的数据拼接。
    uint32_t nIgnoredFrameCounter;  //用于计数在读取过程中被忽略的帧数量，帮助调试或统计数据处理情况。
    uint32_t nIgnoredFlvFrameCounter;   //用于计数在读取过程中被忽略的帧数量，帮助调试或统计数据处理情况。
  } RTMP_READ;
  //​在 RTMP 协议中，客户端和服务器之间通过调用方法进行通信。RTMP_METHOD 结构体用于存储这些方法的名称和对应的编号，便于在通信过程中进行方法的识别和调用。
  typedef struct RTMP_METHOD
  {
    AVal name;  //方法的名称，使用 AVal 结构体表示，包含方法名的字符串及其长度
	int num;    //方法的调用编号或标识符。​
  } RTMP_METHOD;

  typedef struct RTMP
  {
    int m_inChunkSize;  //表示接受时时候块大小
    int m_outChunkSize; //表示发送时使用的块大小
	int m_nBWCheckCounter;  // 带宽检测计数器
    int m_nBytesIn; // 接收数据计数器
    int m_nBytesInSent; // 当前数据已回应计数器
    int m_nBufferMS;    //m_nBufferMS 表示缓冲区的持续时间，单位为毫秒。
    int m_stream_id;		/* 当前连接的流ID returned in _result from createStream, 音视频流共用一个stream id*/
    int m_mediaChannel; // 当前连接媒体使用的块流ID
    uint32_t m_mediaStamp;  // 当前连接媒体最新的时间戳
	uint32_t m_pauseStamp;  // 当前连接媒体暂停时的时间戳
    int m_pausing;  // 是否暂停状态
    int m_nServerBW;    //​m_nServerBW 表示服务器的带宽，单位为字节每秒
    int m_nClientBW;    //m_nClientBW 表示客户端的带宽，单位为字节每秒。
    uint8_t m_nClientBW2;  // 客户端带宽调节方式
    uint8_t m_bPlaying; // 当前是否推流或连接中
    uint8_t m_bSendEncoding;    // 连接服务器时发送编码
    uint8_t m_bSendCounter; // 设置是否向服务器发送接收字节应答

    int m_numInvokes;   // 0x14命令远程过程调用计数
    int m_numCalls; // 0x14命令远程过程请求队列数量
    RTMP_METHOD *m_methodCalls;	// 远程过程调用请求队列

	//在 RTMP 协议中，发送数据时需要缓存每个通道（Chunk Stream ID，简称 csid）上最近一次发送的 RTMPPacket。
	//这样做的目的是：支持 RTMP Chunk Header 的压缩机制（即同一个 csid 的下一次发送可以省略 header 的某些部分，需要参考上一次的包）。
    int m_channelsAllocatedIn;  //最多分配了多少接收通道（Chunk Stream ID）	用于接收服务器发来的 Chunk，维护历史数据
	int m_channelsAllocatedOut; //最多分配了多少发送通道（Chunk Stream ID）	用于发送给服务器的 Chunk，支持 header 压缩
    RTMPPacket **m_vecChannelsIn;   // 对应CSID上一次接收的报文(不同CSID有不同的报文，因此设置为指针数组)
    RTMPPacket **m_vecChannelsOut;  // 对应CSID上一次发送的报文(不同CSID有不同的报文，因此设置为指针数组)
	int* m_channelTimestamp;	// 对应块流ID媒体的最新时间戳

    double m_fAudioCodecs;	// 音频编码器代码
    double m_fVideoCodecs;	// 视频编码器代码
    double m_fEncoding;		/* AMF0 or AMF3 */

    double m_fDuration;		// 当前媒体的时长

    int m_msgCounter;		// 使用HTTP协议发送请求的计数器
    int m_polling;  // 使用HTTP协议接收消息主体时的位置
    int m_resplen;  // 使用HTTP协议接收消息主体时的未读消息计数
    int m_unackd;   // 使用HTTP协议处理时无响应的计数
    AVal m_clientID;    // 使用HTTP协议处理时的身份ID

    RTMP_READ m_read;   // RTMP_Read()操作的上下文
    RTMPPacket m_write; // RTMP_Write()操作使用的可复用报文对象
    RTMPSockBuf m_sb;   // RTMP_ReadPacket()读包操作的上下文
    RTMP_LNK Link;   // RTMP连接上下文
  } RTMP;

  /// <summary>
  /// 解析URL，得到协议名称(protocol)，主机名称(host)，应用程序名称(app)
  /// 示例：rtmp://localhost:1935/vod/mp4:sample1_1500kbps.f4v
  /// </summary>
  /// <param name="url"></param>
  /// <param name="protocol"></param>
  /// <param name="host"></param>
  /// <param name="port"></param>
  /// <param name="playpath"></param>
  /// <param name="app"></param>
  /// <returns>返回true或者false</returns>
  int RTMP_ParseURL(const char *url, int *protocol, AVal *host,
		     unsigned int *port, AVal *playpath, AVal *app);

  void RTMP_ParsePlaypath(AVal *in, AVal *out);
  void RTMP_SetBufferMS(RTMP *r, int size);
  void RTMP_UpdateBufferMS(RTMP *r);

  int RTMP_SetOpt(RTMP *r, const AVal *opt, AVal *arg);
  /// <summary>
  /// 解析URL，根据解析内容设置RTMP
  /// </summary>
  /// <param name="r">in/out,rtmp结构体指针</param>
  /// <param name="url">in,被解析的url字符串</param>
  /// <returns></returns>
  int RTMP_SetupURL(RTMP *r, char *url);
  void RTMP_SetupStream(RTMP *r, int protocol,
			AVal *hostname,
			unsigned int port,
			AVal *sockshost,
			AVal *playpath,
			AVal *tcUrl,
			AVal *swfUrl,
			AVal *pageUrl,
			AVal *app,
			AVal *auth,
			AVal *swfSHA256Hash,
			uint32_t swfSize,
			AVal *flashVer,
			AVal *subscribepath,
			AVal *usherToken,
			int dStart,
			int dStop, int bLiveStream, long int timeout);

  int RTMP_Connect(RTMP *r, RTMPPacket *cp);
  struct sockaddr;
  int RTMP_Connect0(RTMP *r, struct sockaddr *svc);
  int RTMP_Connect1(RTMP *r, RTMPPacket *cp);
  int RTMP_Serve(RTMP *r);
  int RTMP_TLS_Accept(RTMP *r, void *ctx);

  int RTMP_ReadPacket(RTMP *r, RTMPPacket *packet);
  int RTMP_SendPacket(RTMP *r, RTMPPacket *packet, int queue);
  int RTMP_SendChunk(RTMP *r, RTMPChunk *chunk);
  int RTMP_IsConnected(RTMP *r);
  int RTMP_Socket(RTMP *r);
  int RTMP_IsTimedout(RTMP *r);
  double RTMP_GetDuration(RTMP *r);
  int RTMP_ToggleStream(RTMP *r);

  int RTMP_ConnectStream(RTMP *r, int seekTime);
  int RTMP_ReconnectStream(RTMP *r, int seekTime);
  void RTMP_DeleteStream(RTMP *r);
  int RTMP_GetNextMediaPacket(RTMP *r, RTMPPacket *packet);
  int RTMP_ClientPacket(RTMP *r, RTMPPacket *packet);

  int RTMP_SendReceiveAudio(RTMP *r,int flag);
  int RTMP_SendReceiveVideo(RTMP *r,int flag);

  void RTMP_Init(RTMP *r);
  void RTMP_Close(RTMP *r);
  RTMP *RTMP_Alloc(void);
  void RTMP_Free(RTMP *r);
  void RTMP_EnableWrite(RTMP *r);

  void *RTMP_TLS_AllocServerContext(const char* cert, const char* key);
  void RTMP_TLS_FreeServerContext(void *ctx);

  int RTMP_LibVersion(void);
  void RTMP_UserInterrupt(void);	/* user typed Ctrl-C */

  int RTMP_SendCtrl(RTMP *r, short nType, unsigned int nObject,
		     unsigned int nTime);

  /* caller probably doesn't know current timestamp, should
   * just use RTMP_Pause instead
   */
  int RTMP_SendPause(RTMP *r, int DoPause, int dTime);
  int RTMP_Pause(RTMP *r, int DoPause);

  int RTMP_FindFirstMatchingProperty(AMFObject *obj, const AVal *name,
				      AMFObjectProperty * p);

  int RTMPSockBuf_Fill(RTMPSockBuf *sb);
  int RTMPSockBuf_Send(RTMPSockBuf *sb, const char *buf, int len);
  int RTMPSockBuf_Close(RTMPSockBuf *sb);

  int RTMP_SendCreateStream(RTMP *r);
  int RTMP_SendSeek(RTMP *r, int dTime);
  int RTMP_SendServerBW(RTMP *r);
  int RTMP_SendClientBW(RTMP *r);
  void RTMP_DropRequest(RTMP *r, int i, int freeit);
  int RTMP_Read(RTMP *r, char *buf, int size);
  int RTMP_Write(RTMP *r, const char *buf, int size);

/* hashswf.c */
  int RTMP_HashSWF(const char *url, unsigned int *size, unsigned char *hash,
		   int age);

#ifdef __cplusplus
};
#endif

#endif
