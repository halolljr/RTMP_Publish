#ifndef LOOPER_H
#define LOOPER_H
#include <deque>
#include <thread>

#include "mediabase.h"
#include "semaphore.h"


namespace LQF
{
//提供通用的消息循环、线程控制、消息投递机制(不涉及音视频帧类消息的缓存控制）
class Looper
{
public:
    /// <summary>
    /// 内部new了一个Semaphored对象，并开辟一个线程用以回调自身Looper的loop函数，将running置为true
    /// </summary>
    /// <param name="deque_max_size">默认置为30</param>
    Looper(const int deque_max_size = 30);
    virtual ~Looper();
    //flush 是否清空消息队列
    void Post(int what, void *data, bool flush = false);
    void Stop();
    /// <summary>
    /// 虚函数，子类可重写
    /// </summary>
    /// <param name="what"></param>
    /// <param name="data"></param>
    virtual void handle(int what, void *data);
private:
	/// <summary>
	/// 虚函数，子类可重写
	/// </summary>
	/// <param name="msg"></param>
	/// <param name="flush"></param>
	virtual void addmsg(LooperMessage *msg, bool flush);
    static void* trampoline(void* p);
    void loop();
protected:
    std::deque< LooperMessage * > msg_queue_;
    std::thread *worker_;
    Semaphore *head_data_available_;
    std::mutex queue_mutex_;
    bool running_;
    int deque_max_size_ = 30;
};

}
#endif // LOOPER_H
