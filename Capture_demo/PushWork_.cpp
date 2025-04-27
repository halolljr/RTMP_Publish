#include "PushWork_.h"
#include <iostream>

PushWork::PushWork() : running(false) {}

PushWork::~PushWork() {
	stop();
}

bool PushWork::init() {
	if (!video_cap.open(std::string(u8"video=Integrated Camera"))){ // 改成你的摄像头名称
		std::cerr << "Failed to open video capture" << std::endl;
		return false;
	}
	if (!audio_cap.open(std::string(u8"audio=麦克风 (Realtek(R) Audio)"))) { // 改成你的麦克风名称
		std::cerr << "Failed to open audio capture" << std::endl;
		return false;
	}
	if (!video_enc.open(video_cap.getWidth(), video_cap.getHeight(), video_cap.getFps(), 400000)) {
		std::cerr << "Failed to open video encoder"<<std::endl;
		return false;
	}
	if (!audio_enc.open(44100, 2, 64000)) {
		std::cerr << "Failed to open audio encoder" << std::endl;
		return false;
	}
	return true;
}

void PushWork::start() {
	running = true;
	audio_Capture_Thread = std::thread(&PushWork::audioCaptureThread, this);
	audio_Encode_Thread = std::thread(&PushWork::audioEncodeThread, this);
	video_Capture_Thread = std::thread(&PushWork::videoCaptureThread, this);
	video_Encode_Thread = std::thread(&PushWork::videoEncodeThread, this);
}

void PushWork::stop() {
	running = false;
	audio_queue_cv.notify_all();
	video_queue_cv.notify_all();
	if (audio_Capture_Thread.joinable()) {
		audio_Capture_Thread.join();
	}
	if (audio_Encode_Thread.joinable()) {
		audio_Encode_Thread.join();
	}
	if (video_Capture_Thread.joinable()) {
		video_Capture_Thread.join();
	}
	if (video_Encode_Thread.joinable()) {
		video_Encode_Thread.join();
	}
}

void PushWork::audioCaptureThread() {
	while (running) {
		AVFrame* pcm = audio_cap.captureFrame();
		if (pcm) {
			std::unique_lock<std::mutex> lock(audio_queue_mutex);
			audio_frame_queue.push(pcm);
			audio_queue_cv.notify_one();
		}
		// 采集线程尽量快，不sleep
	}
}

void PushWork::audioEncodeThread() {
	while (running) {
		AVFrame* frame = nullptr;
		{
			std::unique_lock<std::mutex> lock(audio_queue_mutex);
			audio_queue_cv.wait(lock, [&] { return !audio_frame_queue.empty() || !running; });
			if (!audio_frame_queue.empty()) {
				frame = audio_frame_queue.front();
				audio_frame_queue.pop();
			}
		}

		if (frame) {
			AVPacket* pkt = audio_enc.encode(frame);
			av_frame_free(&frame); // 记得释放frame
			if (pkt) {
				// TODO: 推流/保存
				std::cout << "[Audio] Encoded packet size: " << pkt->size << std::endl;
				av_packet_free(&pkt);
			}
		}
	}
}

void PushWork::videoCaptureThread() {
	while (running) {
		AVFrame* yuv = video_cap.captureFrame();
		if (yuv) {
			std::unique_lock<std::mutex> lock(video_queue_mutex);
			video_frame_queue.push(yuv);
			video_queue_cv.notify_one();
		}
	}
}

void PushWork::videoEncodeThread() {
	while (running) {
		AVFrame* frame = nullptr;
		{
			std::unique_lock<std::mutex> lock(video_queue_mutex);
			video_queue_cv.wait(lock, [&] { return !video_frame_queue.empty() || !running; });
			if (!video_frame_queue.empty()) {
				frame = video_frame_queue.front();
				video_frame_queue.pop();
			}
		}

		if (frame) {
			AVPacket* pkt = video_enc.encode(frame);
			av_frame_free(&frame);
			if (pkt) {
				// TODO: 推流/保存
				std::cout << "[Video] Encoded packet size: " << pkt->size << std::endl;
				av_packet_free(&pkt);
			}
		}
	}
}

