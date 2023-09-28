#include <atomic>
#include <chrono>
#include <cstdint>
#include <ctime>
#include <functional>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <mutex>
#include <optional>
#include <pthread.h>
#include <string>
#include <thread>

#include <sys/time.h>

#include <jni.h>
#include <android/log.h>

#define LOGI(...) \
  ((void)__android_log_print(ANDROID_LOG_INFO, kTAG, __VA_ARGS__))
#define LOGW(...) \
  ((void)__android_log_print(ANDROID_LOG_WARN, kTAG, __VA_ARGS__))
#define LOGE(...) \
  ((void)__android_log_print(ANDROID_LOG_ERROR, kTAG, __VA_ARGS__))

namespace {

void log(char lvl, const std::string& str)
{
    static const char * kTAG  { "NDKstaff" };
    switch (lvl)
    {
        case 'e':
            LOGE("%s", str.c_str());
            break;
        case 'w':
            LOGW("%s", str.c_str());
            break;
        case 'i':
        default:
            LOGI("%s", str.c_str());
    }
}


} // namespace

template <typename T>
struct Finally
{
    Finally(T runnable)
        : m_runnable{std::move(runnable)}
    {
    }

    ~Finally()
    {
        m_runnable();
    }

    T m_runnable;
};

class Logger
{
public:
    explicit Logger(std::string path)
    {
        path.push_back('/');
        std::string str_time;
        str_time.reserve(24);
        str_time.resize(24);
        std::time_t  t  =  std::time(nullptr);
        auto  szTime  =  std::strftime(&str_time.front(),  24,  "%Y-%m-%d_%H-%M-%S",  std::localtime(&t));
        str_time.resize(szTime);
        path.append(str_time).append(".txt");
        m_logfs.open(path,  std::ofstream::out);
    }

    void Log(char lvl, const std::string& str)
    {
        if (str.empty()) return;
        std::lock_guard lg{mutex};

        log(lvl, str); // Android logcat

        std::time_t  t  =  std::time(nullptr);
        std::tm  tm  =  *std::localtime(&t);

        m_logfs << std::put_time(&tm, "[%Y-%m-%d %H:%M:%S][");
        m_logfs << lvl;
        m_logfs << "][";
        m_logfs << std::to_string(std::hash<std::thread::id>{}(std::this_thread::get_id()));
        m_logfs << "]:";
        m_logfs << str;
        m_logfs << std::endl;

        if ('e' == lvl)
        {
            m_logfs.flush();
        }
    }

private:
    std::ofstream m_logfs;
    std::mutex mutex;
};

class Tester
{
public:
    Tester(std::string logFolder)
    :  m_logger{std::move(logFolder)}
    , m_worker5sec([&, logFolder] { Work5sec(); })
    , m_workerCondVar([&, logFolder]
    {
        if (CondInit())
        {
            WorkCondVar();
        }
    })
    {}

    void Stop()
    {
        m_keepRun = false;
        if (m_worker5sec.joinable())
        {
            m_worker5sec.join();
        }
        if (m_workerCondVar.joinable())
        {
            Signal();
            m_workerCondVar.join();
        }
    }

private:
    void Signal()
    {
        const int signal_rv = pthread_cond_signal(&m_condVar);
        if (signal_rv)
        {
            m_logger.Log('e', "Fail Signal()");
        }
    }

    bool CondInit()
    {
        pthread_condattr_t attr;
        auto rc = pthread_condattr_init(&attr);
        if (pthread_condattr_init(&attr))
        {
            m_logger.Log('e', "Fail pthread_condattr_init, rc=" + rc);
            return false;
        }

        Finally finally
        {
            [&]()
            {
                pthread_condattr_destroy(&attr);
            }
        };

        m_clock = CLOCK_REALTIME;
        // m_clock = CLOCK_MONOTONIC;
        rc = pthread_condattr_setclock(&attr, m_clock);

        if (rc != 0)
        {
            m_logger.Log('e', "Fail pthread_condattr_setclock, rc=" + rc);
            return false;
        }

        rc = pthread_cond_init(&m_condVar, &attr);
        if (rc != 0)
        {
            m_logger.Log('e', "Fail pthread_condattr_setclock, rc=" + rc);
            return false;
        }

        return true;
    }

    void WorkCondVar()
    {
        while (m_keepRun.load())
        {
            struct timespec t;
            int res = clock_gettime(m_clock, &t);
            if (res != 0)
            {
                m_logger.Log('e', "WorkCondVar: FAIL clock_gettime()");
                return;
            }
            t.tv_sec += 5 * 60;

            m_logger.Log('w', "WorkCondVar: before pthread_cond_timedwait");
            pthread_cond_timedwait(&m_condVar, &m_mutex, &t);
            m_logger.Log('w', "WorkCondVar: after pthread_cond_timedwait");
        }
        m_logger.Log('e', "WorkCondVar finished");
    }

    void Work5sec()
    {
        while (m_keepRun.load())
        {
            std::this_thread::sleep_for(std::chrono::seconds(5));
            m_logger.Log('w', "Work5sec");
        }
        m_logger.Log('e', "Work5sec thread finished");
    }

private:
    std::atomic_bool m_keepRun{true};
    Logger m_logger;
    std::thread m_worker5sec;
    std::thread m_workerCondVar;
    pthread_cond_t m_condVar;
    pthread_mutex_t m_mutex;
    clockid_t m_clock;
};

namespace {

std::optional<Tester> tester;

} // namespace

extern "C" JNIEXPORT jstring JNICALL
Java_com_example_test_1monotonic_MainActivity_stringFromJNI(
        JNIEnv* env,
        jobject /* this */)
{
    std::string hello = "Hello from C++";
    return env->NewStringUTF(hello.c_str());
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_test_1monotonic_MainActivity_startTester(
        JNIEnv* env,
        jobject /* this */,
        jstring logFolder)
{
    const char *c_logFolder = env->GetStringUTFChars(logFolder, 0);
    tester.emplace(std::string(c_logFolder, env->GetStringUTFLength(logFolder)));
    env->ReleaseStringUTFChars(logFolder, c_logFolder);
}

extern "C" JNIEXPORT void JNICALL
Java_com_example_test_1monotonic_MainActivity_stopTester(
        JNIEnv* env,
        jobject /* this */) {
    if (tester)
    {
        tester->Stop();
    }
}