#include "PushWork_.h"
#include <iostream>

PushWork::PushWork(){}

PushWork::~PushWork() {
	Close();
}

bool PushWork::Init() {
	if (!video_cap.Open(std::string(u8"video=Integrated Camera"))){ // �ĳ��������ͷ����
		std::cerr << "Failed to open video capture" << std::endl;
		return false;
	}
	video_cap.Set_Callback(std::move(
			std::bind(&PushWork::write_video_frame, this, std::placeholders::_1,
			std::placeholders::_2,std::placeholders::_3,std::placeholders::_4)
	));
	if (!audio_cap.Open(std::string(u8"audio=��˷� (Realtek(R) Audio)"))) { // �ĳ������˷�����
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

	pFrameYUV = av_frame_alloc();
	if (!pFrameYUV) {
		std::cerr << "Failed to alloc for pFrameYUV" << std::endl;
		return false;
	}
	int buffer_size = av_image_get_buffer_size(video_enc.GetAVPixelFormat(), video_enc.GetWidth(), video_enc.GetHeight(), 1);
	m_out_buffer = (uint8_t*)av_malloc(buffer_size);
	//�� m_out_buffer ָ������ pFrameYUV->data[]������� linesize[]��
	//���� pFrameYUV ������ʵ�ʵ�ͼ������ָ�룬�������ڱ��������������
	av_image_fill_arrays(pFrameYUV->data, pFrameYUV->linesize, m_out_buffer, video_enc.GetAVPixelFormat(), video_enc.GetWidth(), video_enc.GetHeight(), 1);
	
	return true;
}

void PushWork::Start() {
	start_time = av_gettime();
	video_cap.Start();
	audio_cap.Start();
}

void PushWork::Stop()
{
	//��Ҫ�ڸ��ԵĲ����߳�������һ�����ߵĻ��ƣ�ֱ������restartȥ����
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
	audio_cap.Close();
	video_enc.close();
	audio_enc.close();
	muxer_->Close();
	if (img_convert_ctx) {
		sws_freeContext(img_convert_ctx);
		img_convert_ctx = nullptr;
	}
	if (aud_convert_ctx) {
		swr_free(&aud_convert_ctx);
	}
	if (m_out_buffer) {
		av_free(m_out_buffer);
		m_out_buffer = nullptr;
	}
	if (pFrameYUV) {
		av_frame_free(&pFrameYUV);  // �Զ���ָ����Ϊ nullptr
	}
}

int PushWork::write_video_frame(AVStream* input_st, enum AVPixelFormat pix_fmt, AVFrame* pframe, int64_t lTimeStramp)
{
	lTimeStramp = lTimeStramp - start_time;
	if (!muxer_) {
		return -1;
	}

	//��������зֱ��ʻ��߸�ʽ�����仯��
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

//����Ǵ��ļ��Ӷ�ȡ���ݲ�����Ļ�
//-----����ǰ
//	pts��0��ʼ���ۼ�video_frame_duration/audio_frame_duration,���߶�Ҫ����AV_TIME_BASE��ʾ��΢��Ϊ��λ
//	pts = av_rescale_q(pts, AVRational{ 1, (int)AV_TIME_BASE }, codec_ctx_->time_base);
//	frame_->pts = pts;
//-----д���ļ�ǰ
// 	src_time_base�Ǳ�������time_base,dst_time_base���������time_base����ͬ�������time_base��һ����ͬ���ᱻԤ�����úõģ�
//  packet->pts = av_rescale_q(packet->pts, src_time_base, dst_time_base);
//	packet->dts = av_rescale_q(packet->dts, src_time_base, dst_time_base);
//	packet->duration = av_rescale_q(packet->duration, src_time_base, dst_time_base);

#if 0
		//Write PTS
		AVRational time_base = video_st->time_base;//{ 1, 1000 };
		AVRational r_framerate1 = input_st->r_frame_rate;//{ 50, 2 }; 
		//Duration between 2 frames (us)
	   // int64_t calc_duration = (double)(AV_TIME_BASE)*(1 / av_q2d(r_framerate1));	//�ڲ�ʱ���
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
		
		muxer_->sendPacket(&enc_pkt);
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
	return 0;
}

