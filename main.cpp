#include <stdio.h>

#include <unistd.h>
#include <pthread.h>

// *) 引入该头文件即可, 不需要在修改任何代码了
#define BACKWARD_HAS_DW 1
#include "dead_lock_stub.h"

class DeadLockCreater
{
public:
    DeadLockCreater()
    {
    
    }

    void on_work_1()
    {
        std::lock_guard<std::mutex> m0(mutex_1);
        sleep(1);
        std::lock_guard<std::mutex> m1(mutex_2);
    }

    void on_work_2()
    {
        std::lock_guard<std::mutex> m0(mutex_2);
        sleep(1);
        std::lock_guard<std::mutex> m1(mutex_1);
    }

    void run_1()
    {
        m_thread_1 = new std::thread(std::bind(&DeadLockCreater::on_work_1, this));
    }

    void run_2()
    {
        m_thread_2 = new std::thread(std::bind(&DeadLockCreater::on_work_2, this));
    }

    void stop_1()
    {
        m_thread_1->join();
        delete m_thread_1;
    }

    void stop_2()
    {
        m_thread_2->join();
        delete m_thread_2;
    }

private:
    std::thread* m_thread_1;
    std::thread* m_thread_2;

    std::mutex mutex_1;
    std::mutex mutex_2;
};  


int main()
{
    // *) 添加该行， 表示启动死锁检测功能 
    DeadLockGraphic::getInstance().start_check();

    DeadLockCreater dlc;

    dlc.run_1();
    dlc.run_2();

    dlc.stop_1();
    dlc.stop_2();

    return 0;
}

