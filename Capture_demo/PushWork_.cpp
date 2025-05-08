#include "PushWork_.h"
#include <iostream>

PushWork::PushWork(){}

PushWork::~PushWork() {
	//Close();
}

static std::string AnsiToUTF8(const char* _ansi, int _ansi_len)
{
	std::string str_utf8("");
	wchar_t* pUnicode = NULL;
	BYTE* pUtfData = NULL;
	do
	{
		int unicodeNeed = MultiByteToWideChar(CP_ACP, 0, _ansi, _ansi_len, NULL, 0);
		pUnicode = new wchar_t[unicodeNeed + 1];
		memset(pUnicode, 0, (unicodeNeed + 1) * sizeof(wchar_t));
		int unicodeDone = MultiByteToWideChar(CP_ACP, 0, _ansi, _ansi_len, (LPWSTR)pUnicode, unicodeNeed);

		if (unicodeDone != unicodeNeed)
		{
			break;
		}

		int utfNeed = WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)pUnicode, unicodeDone, (char*)pUtfData, 0, NULL, NULL);
		pUtfData = new BYTE[utfNeed + 1];
		memset(pUtfData, 0, utfNeed + 1);
		int utfDone = WideCharToMultiByte(CP_UTF8, 0, (LPWSTR)pUnicode, unicodeDone, (char*)pUtfData, utfNeed, NULL, NULL);

		if (utfNeed != utfDone)
		{
			break;
		}
		str_utf8.assign((char*)pUtfData);
	} while (false);

	if (pUnicode)
	{
		delete[] pUnicode;
	}
	if (pUtfData)
	{
		delete[] pUtfData;
	}

	return str_utf8;
}

bool PushWork::Init() {

	std::string video_device_name = "video=Integrated Camera";
	std::string video_device_name_utf_8 = AnsiToUTF8(video_device_name.c_str(), video_device_name.length());

	std::string audio_device_name = "audio=耳机 (AirPods4 - Find My)";
	std::string audio_device_name_utf_8 = AnsiToUTF8(audio_device_name.c_str(), audio_device_name.length());

	if (!video_cap.Open(video_device_name_utf_8)) { // 改成你的摄像头名称
		std::cerr << "Failed to open video capture" << std::endl;
		return false;
	}
	video_cap.Set_Callback(std::move(
			std::bind(&PushWork::write_video_frame, this, std::placeholders::_1,
			std::placeholders::_2,std::placeholders::_3,std::placeholders::_4)
	));

	if (!audio_cap.Open(audio_device_name_utf_8)) { // 改成你的麦克风名称
		std::cerr << "Failed to open audio capture" << std::endl;
		return false;
	}
	audio_cap.Set_Callback(std::move(
		std::bind(&PushWork::wirte_audio_frame,this,std::placeholders::_1,
			std::placeholders::_2,std::placeholders::_3)
	));

	if (!video_enc.open(video_cap.getWidth(), video_cap.getHeight(), video_cap.getFps(), 3500000)) {
		std::cerr << "Failed to open video encoder"<<std::endl;
		return false;
	}

	if (!audio_enc.open(44100, 2, 128000)) {
		std::cerr << "Failed to open audio encoder" << std::endl;
		return false;
	}

	muxer_ = new Muxer();
	if (!muxer_->Open("dump.mp4" , video_enc.GetCodecContext(), audio_enc.GetCodecContext())) {
		std::cerr << "Failed to open Muxer" << std::endl;
		return false;
	}

	//视频帧转化的初始化设置
	pFrameYUV = av_frame_alloc();
	if (!pFrameYUV) {
		std::cerr << "Failed to alloc for pFrameYUV" << std::endl;
		return false;
	}
	int buffer_size = av_image_get_buffer_size(video_enc.GetAVPixelFormat(), video_enc.GetWidth(), video_enc.GetHeight(), 1);
	m_out_buffer = (uint8_t*)av_malloc(buffer_size);
	//把 m_out_buffer 指针填入 pFrameYUV->data[]，并填充 linesize[]。
	//这样 pFrameYUV 就有了实际的图像数据指针，可以用于编码或其他操作。
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, m_out_buffer, video_enc.GetAVPixelFormat(), video_enc.GetWidth(), video_enc.GetHeight(), 1);
	
	//音频帧重采样的初始化设置
	m_fifo = av_audio_fifo_alloc(audio_enc.GetCodecContext()->sample_fmt, audio_enc.GetCodecContext()->channels, 1);
	//只是分配了channels个通道数组指针
	//并没有为每个通道分配实际的音频样本缓冲区
	if (!(m_converted_input_samples = (uint8_t**)calloc(audio_enc.GetCodecContext()->channels, sizeof(**m_converted_input_samples)))) {
		std::cerr << "Could not allocate converted_input_samples**" << std::endl;
		return false;
	}
	m_converted_input_samples[0] = nullptr;
	
	return true;
}

void PushWork::Start() {
	start_time = av_gettime();
	video_cap.Start();
	audio_cap.Start();
}

void PushWork::Stop()
{
	//需要在各自的捕获线程里面做一个休眠的机制，直到启用restart去唤醒
	video_cap.Stop();
	audio_cap.Stop();
}

void PushWork::Restart()
{
	start_time = av_gettime();
}

void PushWork::Close()
{
	video_cap.Close();
	video_enc.close();
	audio_cap.Close();
	audio_enc.close();
	if (nullptr != muxer_) {
		muxer_->Close();
	}
	if (img_convert_ctx) {
		sws_freeContext(img_convert_ctx);
		img_convert_ctx = nullptr;
	}
	if (pFrameYUV) {
		av_frame_free(&pFrameYUV);  // 自动将指针设为 nullptr
	}
	if (m_out_buffer) {
		av_free(m_out_buffer);
		m_out_buffer = nullptr;
	}

	if (aud_convert_ctx) {
		swr_free(&aud_convert_ctx);
	}
	if (m_fifo) {
		av_audio_fifo_free(m_fifo);
		m_fifo = nullptr;
	}
	if (m_converted_input_samples) {
		//释放每个通道的buffer
		av_freep(&m_converted_input_samples[0]);
		//释放通道数组本身
		//free(m_converted_input_samples);
		m_converted_input_samples = nullptr;
	}
}

int PushWork::write_video_frame(AVStream* input_st, enum AVPixelFormat pix_fmt, AVFrame* pframe, int64_t lTimeStramp)
{
	lTimeStramp = lTimeStramp - start_time;
	if (!muxer_) {
		return -1;
	}

	//如果过程中分辨率或者格式发生变化了
	//if (!img_convert_ctx || last_src_width != cur_width || last_src_height != cur_height || last_src_format != pix_fmt) {
	//	if (img_convert_ctx)
	//		sws_freeContext(img_convert_ctx);
	//	img_convert_ctx = sws_getContext(video_cap.getWidth(), video_cap.getHeight(), pix_fmt,
	//		video_enc.GetWidth(), video_enc.GetHeight(), video_enc.GetAVPixelFormat(), SWS_BICUBIC, NULL, NULL, NULL);
	//	if (!img_convert_ctx) {
	//		std::cerr << __FUNCTION__ << " : Failed to Creat a SwsContext" << std::endl;
	//		return -1;
	//	}
	//}
	
	if (!img_convert_ctx) {
		img_convert_ctx = sws_getContext(video_cap.getWidth(), video_cap.getHeight(), pix_fmt,
			video_enc.GetWidth(), video_enc.GetHeight(), video_enc.GetAVPixelFormat(), SWS_BICUBIC, NULL, NULL, NULL);
		if (!img_convert_ctx) {
			std::cerr << __FUNCTION__ << " : Failed to Creat a SwsContext" << std::endl;
			return -1;
		}
	}
	sws_scale(img_convert_ctx, (const uint8_t* const*)pframe->data, pframe->linesize, 0, video_enc.GetHeight(), pFrameYUV->data, pFrameYUV->linesize);
	
	//--------------FIX ME!!!!!!!!!!!--------------
	//pFrameYUV->width = pframe->width;
	//pFrameYUV->height = pframe->height;
	
	pFrameYUV->width = video_enc.GetWidth();
	pFrameYUV->height = video_enc.GetHeight();
	pFrameYUV->format = video_enc.GetAVPixelFormat();
	
	enc_pkt.data = nullptr;
	enc_pkt.size = 0;
	av_init_packet(&enc_pkt);

	if (video_enc.encode(pFrameYUV, &enc_pkt)) {
		enc_pkt.stream_index = muxer_->Get_Video_AVStream()->index;

//如果是从文件从读取数据并编码的话
//-----编码前
//	pts从0开始并累加video_frame_duration/audio_frame_duration,二者都要乘以AV_TIME_BASE表示以微妙为单位
//	pts = av_rescale_q(pts, AVRational{ 1, (int)AV_TIME_BASE }, codec_ctx_->time_base);
//	frame_->pts = pts;
//-----写入文件前
// 	src_time_base是编码器的time_base,dst_time_base是输出流的time_base（不同输出流的time_base不一样且同样会被预先设置好的）
//  packet->pts = av_rescale_q(packet->pts, src_time_base, dst_time_base);
//	packet->dts = av_rescale_q(packet->dts, src_time_base, dst_time_base);
//	packet->duration = av_rescale_q(packet->duration, src_time_base, dst_time_base);

#if 0
		//Write PTS
		AVRational time_base = video_st->time_base;//{ 1, 1000 };
		AVRational r_framerate1 = input_st->r_frame_rate;//{ 50, 2 }; 
		//Duration between 2 frames (us)
	   // int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//内部时间戳
		int64_t calc_pts = (double)m_vid_framecnt * (AV_TIME_BASE) * (1 / av_q2d(r_framerate1));

		//Parameters
		enc_pkt.pts = av_rescale_q(calc_pts, time_base_q, time_base);  //enc_pkt.pts = (double)(framecnt*calc_duration)*(double)(av_q2d(time_base_q)) / (double)(av_q2d(time_base));
		enc_pkt.dts = enc_pkt.pts;
		//enc_pkt.duration = av_rescale_q(calc_duration, time_base_q, time_base); //(double)(calc_duration)*(double)(av_q2d(time_base_q)) / (double)(av_q2d(time_base));
		//enc_pkt.pos = -1;
#else
		enc_pkt.pts= av_rescale_q(lTimeStramp, video_time_base_q, muxer_->Get_Video_AVStream()->time_base);
		enc_pkt.dts = enc_pkt.pts;
#endif
		m_vid_framecnt++;
		
		//Delay
		//int64_t pts_time = av_rescale_q(enc_pkt.pts, time_base, time_base_q);
		//int64_t now_time = av_gettime() - start_time;						
		//if ((pts_time > now_time) && ((vid_next_pts + pts_time - now_time)<aud_next_pts))
		//av_usleep(pts_time - now_time);
		mtx.lock();
		muxer_->sendPacket(&enc_pkt);
		mtx.unlock();
	}
	else {
		std::cerr << "Failed to encode frame" << std::endl;
	}
	av_packet_unref(&enc_pkt);
	return 0;
}

int PushWork::wirte_audio_frame(AVStream* input_st, AVFrame* input_frame, int64_t lTimeStramp)
{
	lTimeStramp = lTimeStramp - start_time;
	if (!muxer_) {
		return -1;
	}

	//AAC编码器一般是1024个采样点编成一帧
	const int out_put_frame_size = audio_enc.GetCodecContext()->frame_size;
	
	int nFifoSamples = av_audio_fifo_size(m_fifo);
	//缓冲区剩余点数的时间（微秒）
	int64_t timeshift = (int64_t)nFifoSamples * AV_TIME_BASE / (int64_t)(input_st->codecpar->sample_rate);
	
	m_aud_framecnt += input_frame->nb_samples;
	
	if (nullptr == aud_convert_ctx) {
		aud_convert_ctx = swr_alloc_set_opts(nullptr,
			av_get_default_channel_layout(audio_enc.GetCodecContext()->channels),
			audio_enc.GetCodecContext()->sample_fmt,
			audio_enc.GetCodecContext()->sample_rate,
			av_get_default_channel_layout(input_st->codecpar->channels),
			(enum AVSampleFormat)input_st->codecpar->format,
			input_st->codecpar->sample_rate,
			0, nullptr);
		if (!aud_convert_ctx) {
			std::cerr << "Failed to create a SwrContext" << std::endl;
			return -1;
		}
		int ret = swr_init(aud_convert_ctx);
		if (ret < 0) {
			char errbuf[1024] = { 0 };
			av_strerror(ret, errbuf, sizeof(errbuf) - 1);
			std::cerr << "swr_init() Failed:" << errbuf << std::endl;
			return -1;
		}
	}
	// 估算输出采样数
	int max_dst_nb_samples = av_rescale_rnd(
		swr_get_delay(aud_convert_ctx, input_st->codecpar->sample_rate) + input_frame->nb_samples,
		audio_enc.GetCodecContext()->sample_rate,
		input_st->codecpar->sample_rate,
		AV_ROUND_UP);

	if (av_samples_alloc(m_converted_input_samples, nullptr, audio_enc.GetCodecContext()->channels,
		max_dst_nb_samples, audio_enc.GetCodecContext()->sample_fmt, 0) < 0) {
		std::cerr << "Could not allocate audio buffer with av_samples_alloc" << std::endl;
		return -1;
	}

	// 重采样
	int out_put_samples = swr_convert(aud_convert_ctx, m_converted_input_samples, max_dst_nb_samples,
		(const uint8_t**)input_frame->extended_data, input_frame->nb_samples);
	if (out_put_samples < 0) {
		std::cerr << "Failed to swr_convert" << std::endl;
		return -1;
	}
	if (av_audio_fifo_realloc(m_fifo, av_audio_fifo_size(m_fifo) + out_put_samples) < 0) {
		std::cerr << "Failed to av_audio_fifo_realloc" << std::endl;
		return -1;
	}
	// 写入 FIFO
	int written = av_audio_fifo_write(m_fifo, (void**)m_converted_input_samples, out_put_samples);
	if (written < out_put_samples) {
		std::cerr << "FIFO buffer overflow, written less than expected" << std::endl;
		return -1;
	}
	int64_t timeinc = (int64_t)audio_enc.GetCodecContext()->frame_size * AV_TIME_BASE / (int64_t)(input_st->codecpar->sample_rate);
	if (lTimeStramp - timeshift > m_nLastAudioPresentationTime) {
		m_nLastAudioPresentationTime = lTimeStramp - timeshift;
	}
	while (av_audio_fifo_size(m_fifo) >= out_put_frame_size) {
		AVFrame* out_put_frame = av_frame_alloc();
		if (nullptr == out_put_frame) {
			std::cerr << __FUNCTION__ << " : Failed to av_frame_alloc()" << std::endl;
			return -1;
		}
		const int frame_size = FFMIN(av_audio_fifo_size(m_fifo), out_put_frame_size);
		out_put_frame->nb_samples = frame_size;
		out_put_frame->channel_layout = audio_enc.GetCodecContext()->channel_layout;
		out_put_frame->format = audio_enc.GetCodecContext()->sample_fmt;
		out_put_frame->sample_rate = audio_enc.GetCodecContext()->sample_rate;
		if (av_frame_get_buffer(out_put_frame, 0) < 0) {
			std::cerr << "Failed to reallocate output audio frame buffer." << std::endl;
			return -1;
		}
		if (av_audio_fifo_read(m_fifo, (void**)out_put_frame->data, frame_size) < frame_size) {
			std::cerr << "Coudl not read from AVAudioFifo" << std::endl;
			av_frame_free(&out_put_frame);
			return -1;
		}
		av_init_packet(&audio_enc_pkt);
		audio_enc_pkt.data = nullptr;
		audio_enc_pkt.size = 0;
		if (audio_enc.encode(out_put_frame, &audio_enc_pkt)) {
			audio_enc_pkt.stream_index = muxer_->Get_Audio_AVStream()->index;
#if 0
			AVRational r_framerate1 = { input_st->codec->sample_rate, 1 };// { 44100, 1};
			//int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));  //内部时间戳
			int64_t calc_pts = (double)m_nb_samples * (AV_TIME_BASE) * (1 / av_q2d(r_framerate1));

			output_packet.pts = av_rescale_q(calc_pts, time_base_q, audio_st->time_base);
			//output_packet.dts = output_packet.pts;
			//output_packet.duration = output_frame->nb_samples;
#else
			audio_enc_pkt.pts = av_rescale_q(m_nLastAudioPresentationTime, audio_time_base_q, muxer_->Get_Audio_AVStream()->time_base);
			audio_enc_pkt.dts = audio_enc_pkt.pts;
#endif
			//int64_t pts_time = av_rescale_q(output_packet.pts, time_base, time_base_q);
			//int64_t now_time = av_gettime() - start_time;
			//if ((pts_time > now_time) && ((aud_next_pts + pts_time - now_time)<vid_next_pts))
			//av_usleep(pts_time - now_time);
			mtx.lock();
			muxer_->sendPacket(&audio_enc_pkt);
			mtx.unlock();
			av_packet_unref(&audio_enc_pkt);
		}
		m_out_samples += out_put_frame->nb_samples;
		m_nLastAudioPresentationTime += timeinc;
		av_frame_free(&out_put_frame);
	}
	return 0;
}

