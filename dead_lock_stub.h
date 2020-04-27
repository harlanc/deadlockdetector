#ifndef __DEAD_LOCK_STUB_H__
#define __DEAD_LOCK_STUB_H__

#include <stdio.h>
#include <stdint.h>

#include <unistd.h>
#include <pthread.h>

#include <deque>
#include <vector>
#include <map>
#include <memory>

#include "backward.hpp"
#include "spdlog/spdlog.h"
#include "spdlog/sinks/basic_file_sink.h"

using namespace backward;

struct thread_graphic_vertex_t {
    int indegress;
    std::vector<uint64_t> vertexs;
    
    thread_graphic_vertex_t()
        : indegress(0) {
    }
};

class DeadLockGraphic {

public:
    static DeadLockGraphic &getInstance() {
        static DeadLockGraphic instance;
        return instance;
    }

    void lock_before(uint64_t thread_id, uint64_t lock_addr) {

        std::lock(m_mutex_thread_apply_lock,m_mutex_thread_stacktrace);
        {
            std::lock_guard<std::mutex> m0(m_mutex_thread_apply_lock,std::adopt_lock);
            m_thread_apply_lock[thread_id] = lock_addr;
        }

        //暂时保存堆栈信息，如果后边判断在此处发生死锁，把这里的堆栈打印出来。
        StackTrace st; 
        TraceResolver tr;
        st.load_here(64);
        tr.load_stacktrace(st);

        std::stringstream st_buffer;
        st_buffer << " thread_id " << thread_id
                  << " apply lock_addr " << lock_addr
                  << std::endl;

        for (size_t i = 0; i < st.size(); ++i) {

            ResolvedTrace trace = tr.resolve(st[i]);
            st_buffer << "#" << i
                      // << " " << trace.object_filename
                      // << " " << trace.object_function
                      // << " [" << trace.addr << "]"
                      << "  " << trace.source.filename
                      << "  " << trace.source.function
                      << "  " << trace.source.line
                      << std::endl;
        }    
    
        //std::cout<<st_buffer.str()<<std::endl;
        {
            std::lock_guard<std::mutex> m1(m_mutex_thread_stacktrace,std::adopt_lock);
            m_thread_stacktrace[thread_id] += st_buffer.str();
        }
    }

    void lock_after(uint64_t thread_id, uint64_t lock_addr) {

        std::lock(m_mutex_lock_belong_thread,m_mutex_thread_apply_lock,m_mutex_thread_stacktrace);

        {
            std::lock_guard<std::mutex> m0(m_mutex_thread_apply_lock,std::adopt_lock);
            m_thread_apply_lock.erase(thread_id);
        }

        {
            std::lock_guard<std::mutex> m1(m_mutex_thread_stacktrace,std::adopt_lock);
            m_thread_stacktrace.erase(thread_id);
        }

        {
            std::lock_guard<std::mutex> m2(m_mutex_lock_belong_thread,std::adopt_lock);
            m_lock_belong_thread[lock_addr] = thread_id;
        }
    }
    
    void unlock_after(uint64_t thread_id, uint64_t lock_addr) {

        std::lock_guard<std::mutex> m(m_mutex_lock_belong_thread);
        m_lock_belong_thread.erase(lock_addr);
    }

    void check_dead_lock()
    {
        std::map<uint64_t, uint64_t> lock_belong_thread;
        std::map<uint64_t, uint64_t> thread_apply_lock;
        std::map<uint64_t,std::string> thread_stacktrace;

        std::lock(m_mutex_lock_belong_thread,m_mutex_thread_apply_lock,m_mutex_thread_stacktrace);
     
        {
            std::lock_guard<std::mutex> m0(m_mutex_thread_apply_lock,std::adopt_lock);
            thread_apply_lock = m_thread_apply_lock;
        }

        {
            std::lock_guard<std::mutex> m1(m_mutex_thread_stacktrace,std::adopt_lock);
            thread_stacktrace = m_thread_stacktrace;
        }

        {
            std::lock_guard<std::mutex> m2(m_mutex_lock_belong_thread,std::adopt_lock);
            lock_belong_thread = m_lock_belong_thread;
        }

        // 构建有向图
        std::map<uint64_t, thread_graphic_vertex_t> graphics;
        for ( std::map<uint64_t, uint64_t>::const_iterator iter = thread_apply_lock.begin(); 
                iter != thread_apply_lock.end(); iter++  ) {
            uint64_t thd_id1 = iter->first;
            uint64_t lock_id = iter->second; 
            if ( lock_belong_thread.find(lock_id) == lock_belong_thread.end() ) {
                continue;
            }           
            
            uint64_t thd_id2 = lock_belong_thread[lock_id];
            
            if ( graphics.find(thd_id1) == graphics.end() ) {
                graphics[thd_id1] = thread_graphic_vertex_t();
            }
            if ( graphics.find(thd_id2) == graphics.end() ) {
                graphics[thd_id2] = thread_graphic_vertex_t();
            }

            // 保存有向边
            graphics[thd_id1].vertexs.push_back(thd_id2);
            // 入度 indegress++
            graphics[thd_id2].indegress++;
        }

        int graphicssize = graphics.size();

        // 检测流程一
        uint64_t counter = 0;
        std::deque<uint64_t> graphics_queue;
        for ( std::map<uint64_t, thread_graphic_vertex_t>::const_iterator iter = graphics.begin();
                iter != graphics.end(); iter++ ) {
            uint64_t thd_id = iter->first;
            const thread_graphic_vertex_t &gvert = iter->second;
            if ( gvert.indegress == 0 ) {
                graphics_queue.push_back(thd_id);
                counter ++;
            }
        }
            
        // 检测流程二
        while ( !graphics_queue.empty() ) {

            //取出入度为0的点
            uint64_t thd_id = graphics_queue.front();
            graphics_queue.pop_front();

            //删除边
            const thread_graphic_vertex_t &gvert = graphics[thd_id];
            // 遍历邻近有向边
            for ( size_t i = 0; i < gvert.vertexs.size(); i++ ) {
                uint64_t thd_id2 = gvert.vertexs[i];
                graphics[thd_id2].indegress --;
                if ( graphics[thd_id2].indegress == 0 ) {
                    graphics_queue.push_back(thd_id2);
                    counter++;
                }
            }
            //删除入度为0的点（因为不可能是环的一部分）
            graphics.erase(thd_id);
        }

        // 检测流程三
        if ( counter != graphics.size() ) {

            printf("Found Dead Lock!!!!!!!!!!!!\n");
            //打印所有剩下的入度不为0的点的堆栈信息
            for ( std::map<uint64_t, thread_graphic_vertex_t>::const_iterator iter = graphics.begin();
                iter != graphics.end(); iter++ ) {

                uint64_t thd_id = iter->first;

                spdlog::info(thread_stacktrace[thd_id]);

                std::stringstream lock_belong_info;
                lock_belong_info << " The lock addr " << thread_apply_lock[thd_id]
                  << " is owned by " << lock_belong_thread[thread_apply_lock[thd_id]]
                  << std::endl;
                spdlog::info(lock_belong_info.str());

                m_file_logger->flush();
            }
        } else {
            printf("No Found Dead Lock.\n");
        }        

    }

    void start_check() {
        pthread_t tid;
        pthread_create(&tid, NULL, thread_rountine, (void *)(this));
    }

    static void *thread_rountine(void *args) {
        DeadLockGraphic *ptr_graphics = static_cast<DeadLockGraphic *>(args);
        while ( true ) {
            // 每十秒检测一次
            sleep(10);
            ptr_graphics->check_dead_lock();
        }
    }

private:

    std::mutex m_mutex_lock_belong_thread;
    // lock 对应 线程 拥有者的map
    std::map<uint64_t, uint64_t> m_lock_belong_thread;

    std::mutex m_mutex_thread_apply_lock;
    // 线程尝试去申请的lock map
    std::map<uint64_t, uint64_t> m_thread_apply_lock; 

    std::mutex m_mutex_thread_stacktrace;
    // 保存特定线程获取锁时对应的StackTrace信息
    std::map<uint64_t,std::string> m_thread_stacktrace;

    std::shared_ptr<spdlog::logger> m_file_logger;

 private:
    DeadLockGraphic() {

        m_file_logger = spdlog::basic_logger_mt("basic_logger", "logs/basic.txt");
        spdlog::set_default_logger(m_file_logger);     
    }
    ~DeadLockGraphic() {
        
    }
private:
    DeadLockGraphic(const DeadLockGraphic &) {
    }
    DeadLockGraphic& operator=(const DeadLockGraphic &) {
        return *this;    
    }

private:
    

};

//for c
#include <sys/syscall.h>

#ifdef __linux__
#define gettid() syscall(__NR_gettid)
#else
#define gettid() syscall(SYS_gettid)
#endif

// 拦截lock, 添加before, after操作, 记录锁与线程的关系
#define pthread_mutex_lock(x)                                                                       \
    do {                                                                                            \
        DeadLockGraphic::getInstance().lock_before(gettid(), reinterpret_cast<uint64_t>(x));        \
        pthread_mutex_lock(x);                                                                      \
        DeadLockGraphic::getInstance().lock_after(gettid(), reinterpret_cast<uint64_t>(x));         \
    } while (false);

// 拦截unlock, 添加after操作, 解除锁和线程的关系
#define pthread_mutex_unlock(x)                                                                     \
    do {                                                                                            \
        pthread_mutex_unlock(x);                                                                    \
        DeadLockGraphic::getInstance().unlock_after(gettid(), reinterpret_cast<uint64_t>(x));       \
    } while(false);


//for c++
namespace std{

class DL_Mutex{
    public:
        DL_Mutex(){
            m_mutex = PTHREAD_MUTEX_INITIALIZER;
        }
        void Lock(){
            pthread_mutex_lock(&m_mutex);
        }
        void Unlock(){
            pthread_mutex_unlock(&m_mutex);
        }
    private:
        pthread_mutex_t m_mutex;


};

template <class T>
class DL_LockGuard{
    public:
        DL_LockGuard(T &mutex):m_lockguard_mutex(mutex){
            m_lockguard_mutex.Lock();
        }
        ~DL_LockGuard(){
            m_lockguard_mutex.Unlock();
        }
    private:
        T &m_lockguard_mutex;
};

#define mutex DL_Mutex
#define lock_guard DL_LockGuard


}







#endif
