#pragma once

#include "VideoCapture.h"
#include "AudioCapture.h"
#include "VideoEncoder.h"
#include "AudioEncoder.h"
#include <thread>
#include <atomic>
#include <mutex>
#include <queue>
class PushWork {
public:
	PushWork();
	~PushWork();

	bool init();
	void start();
	void stop();

private:
	void audioCaptureThread();
	void audioEncodeThread();
	void videoCaptureThread();
	void videoEncodeThread();
private:
	VideoCapture video_cap;
	AudioCapture audio_cap;
	VideoEncoder video_enc;
	AudioEncoder audio_enc;
	// 音频相关
	std::queue<AVFrame*> audio_frame_queue;
	std::mutex audio_queue_mutex;
	std::condition_variable audio_queue_cv;

	// 视频相关
	std::queue<AVFrame*> video_frame_queue;
	std::mutex video_queue_mutex;
	std::condition_variable video_queue_cv;

	std::thread audio_Encode_Thread;
	std::thread audio_Capture_Thread;

	std::thread video_Capture_Thread;
	std::thread video_Encode_Thread;

	std::atomic<bool> running;
};
