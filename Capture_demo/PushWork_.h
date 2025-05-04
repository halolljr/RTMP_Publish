#pragma once

#include "VideoCapture.h"
#include "AudioCapture.h"
#include "VideoEncoder.h"
#include "AudioEncoder.h"
#include "Muxer.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
extern "C" {
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
}
class PushWork {
public:
	PushWork();
	~PushWork();

	bool Init();
	void Start();
	void Stop();
	void Restart();
	void Close();
private:
	int write_video_frame(AVStream* input_st, enum AVPixelFormat pix_fmt, AVFrame* pframe, int64_t lTimeStramp);
	int wirte_audio_frame(AVStream* input_st, AVFrame* input_frame, int64_t lTimeStramp);
private:
	Muxer* muxer_ = nullptr;

	VideoCapture video_cap;
	AudioCapture audio_cap;
	
	VideoEncoder video_enc;
	SwsContext* img_convert_ctx = nullptr;
	AVFrame* pFrameYUV = nullptr;
	uint8_t* m_out_buffer = nullptr;
	AVPacket enc_pkt;
	AVRational video_time_base_q = { 1, AV_TIME_BASE };
	int  m_vid_framecnt = 0;

	AudioEncoder audio_enc;
	SwrContext* aud_convert_ctx = nullptr;

	int64_t start_time = 0;
};
