#ifndef SEMAPHORE_H
#define SEMAPHORE_H

#include <mutex>
#include <condition_variable>
#include <algorithm>
using namespace std;
namespace LQF
{
//这个 Semaphore 类实现了一个 信号量（Semaphore）机制，它是一种用于线程间同步的原语，允许线程等待某个条件，并在满足时被唤醒。
class Semaphore
{
public:
    explicit Semaphore(unsigned int initial = 0) {
        count_ = 0;
    }
    ~Semaphore() {

    }
    /// <summary>
    /// 增加 count_，表示释放资源或发出通知
    /// </summary>
    /// <param name="n"></param>
    void post(unsigned int n = 1) {

        unique_lock<mutex> lock(mutex_);
        count_ += n;
        if(n == 1){
            condition_.notify_one();
        }else{
            condition_.notify_all();
        }

    }
    /// <summary>
	/// 如果信号量为 0，就阻塞在条件变量上。被唤醒后减一，表示「消费资源」
    /// </summary>
    void wait() {
        unique_lock<mutex> lock(mutex_);
		/*可以如此写：
			condition_.wait(lock, [] { return count_ > 0; });
		    此时 wait() 会：

			判断 count_ > 0 是否为真；

			如果是假，则释放锁并阻塞；

			唤醒后 自动重新获取锁 并再次判断；

			直到条件为真才返回。*/
        while (count_ == 0) {
			/*释放 mutex_ 锁，让其他线程可以调用 post()。

			然后当前线程进入阻塞状态。

			等到别的线程调用 condition_.notify_one()（在 post() 中），线程被唤醒后会重新获得锁，继续判断条件防止虚假唤醒。
            
            */
            condition_.wait(lock);
        }
        --count_;
    }
private:
    int count_; //表示当前的可用“信号量数量”
    mutex mutex_;
    condition_variable_any condition_;
};
}
#endif // SEMAPHORE_H
