#pragma once

#include "VideoCapture.h"
#include "AudioCapture.h"
#include "VideoEncoder.h"
#include "AudioEncoder.h"
#include "Muxer.h"
#include <windows.h>
#include <mutex>
extern "C" {
#include <libswscale/swscale.h>
#include <libswresample/swresample.h>
#include <libavutil/time.h>
#include <libavutil/imgutils.h>
#include <libavutil/audio_fifo.h>
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
	//输出文件封装
	Muxer* muxer_ = nullptr;
	std::mutex mtx_;

	//捕获麦克风摄像头
	VideoCapture video_cap;
	AudioCapture audio_cap;
	
	//捕获到的摄像头数据再编码
	VideoEncoder video_enc;
	SwsContext* img_convert_ctx = nullptr;
	AVFrame* pFrameYUV = nullptr;
	uint8_t* m_out_buffer = nullptr;
	AVPacket enc_pkt;
	AVRational video_time_base_q = { 1, AV_TIME_BASE };
	int  m_vid_framecnt = 0;

	//捕获到的麦克风数据再编码
	AudioEncoder audio_enc;
	SwrContext* aud_convert_ctx = nullptr;
	AVRational audio_time_base_q = { 1,AV_TIME_BASE };
	AVAudioFifo* m_fifo = nullptr;
	uint8_t** m_converted_input_samples = nullptr;
	AVPacket audio_enc_pkt;
	int m_out_samples = 0;
	int64_t m_nLastAudioPresentationTime = 0;
	int m_aud_framecnt = 0;

	int64_t start_time = 0;
};
