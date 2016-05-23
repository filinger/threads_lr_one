#include <iostream>
#include <vector>
#include <unistd.h>

class Thread {
public:
    virtual ~Thread() { }

    virtual void join() = 0;

    virtual void close() = 0;
};

class Mutex {
public:
    virtual ~Mutex() { }

    virtual void lock() = 0;

    virtual void unlock() = 0;
};

class Semaphore {
public:
    virtual ~Semaphore() { }

    virtual void wait() = 0;

    virtual void post() = 0;
};

#ifdef __GNUC__

#include "semaphore.h"

class PosixThread : public Thread {
public:
    PosixThread(void *(procedure)(void *), void *ctx) {
        pthread_create(&th, NULL, procedure, ctx);
    }

    void join() override {
        pthread_join(th, &result);
    }

    void close() override {
        // Not Implemented
    }

private:
    void *result;
    pthread_t th;
};

class PosixMutex : public Mutex {
public:
    PosixMutex() {
        pthread_mutex_init(&mt, NULL);
    }

    ~PosixMutex() {
        pthread_mutex_destroy(&mt);
    }

    void lock() override {
        pthread_mutex_lock(&mt);
    }

    void unlock() override {
        pthread_mutex_unlock(&mt);
    }

private:
    pthread_mutex_t mt;
};

class PosixSemaphore : public Semaphore {
public:
    PosixSemaphore(const char *name, unsigned int initial, unsigned int max) {
        sm = sem_open(name, initial, max);
    }

    ~PosixSemaphore() {
        sem_close(sm);
    }

    void wait() override {
        sem_wait(sm);
    }

    void post() override {
        sem_post(sm);
    }

private:
    sem_t *sm;
};

#elif defined _WIN32 || defined _WIN64
#include <windows.h>

class WinThread : public Thread {
public:
    WinThread(LPTHREAD_START_ROUTINE routine, void *ctx) {
        hThread = CreateThread(NULL, 0, routine, ctx, 0, &IDThread);
    }

    void join() override {
        WaitForSingleObject(hThread, INFINITE);
    }

    void close() override {
        CloseHandle(hThread);
    }

private:
    HANDLE hThread;
    DWORD IDThread;
};

class WinMutex : public Mutex {
public:
    WinMutex(const char *name) : name(name) {
        mt = CreateMutex(NULL, false, name);
    }

    void lock() override {
        mt = OpenMutex(NULL, false, name);
    }

    void unlock() override {
        ReleaseMutex(mt);
    }

private:
    const char *name;
    HANDLE mt;
};

class WinSemaphore : public Semaphore {
public:
    WinSemaphore(unsigned int initial, unsigned int max) {
        sm = CreateSemaphore(NULL, initial, max, NULL);
    }

    ~WinSemaphore() {
        CloseHandle(sm);
    }

    void wait()	override {
        WaitForSingleObject(sm, 0L);
    }

    void post() override {
        ReleaseSemaphore(sm, 1, NULL);
    }

private:
    HANDLE sm;
};

#endif

struct ThreadContext {
    ThreadContext(const char *ch, const int length) : ch(ch), length(length) { }

    const char *ch;
    const int length;
};

struct MutexedThreadContext : public ThreadContext {
    MutexedThreadContext(const char *ch, const int length, Mutex *mutex) : ThreadContext(ch, length), mutex(mutex) { }

    Mutex *mutex;
};

struct SemaphoreSwitch {
    const char *ch;
    Semaphore *semWait;
    Semaphore *semPost;
};

struct SemaphoredThreadContext : public ThreadContext {
    SemaphoredThreadContext(const char *ch, const int length, const std::vector<SemaphoreSwitch> &switches)
            : ThreadContext(ch, length), switches(switches) { }

    std::vector<SemaphoreSwitch> switches;
};

#ifdef __GNUC__
#define Thread PosixThread

void *printChar(void *arg);

void *printCharMutexed(void *arg);

void *printCharSemaphored(void *arg);

#elif defined _WIN32 || defined _WIN64
#define Thread WinThread

DWORD WINAPI printChar(void *arg);
DWORD WINAPI printCharMutexed(void *arg);
DWORD WINAPI printCharSemaphored(void *arg);

#endif

int main(int argc, char **argv) {
#ifdef __GNUC__
    PosixMutex mutex;
    PosixSemaphore smG("smG", 0, 0);
    PosixSemaphore smH("smH", 0, 1);
#elif defined _WIN32 || defined _WIN64
    WinMutex mutex("mt");
    WinSemaphore smK(0, 0);
    WinSemaphore smM(0, 1);
#endif

    std::vector<SemaphoreSwitch> switches = {
            {"g", &smG, &smH},
            {"h", &smH, &smG}
    };

    Thread thA = Thread(printChar, new ThreadContext("a", 5));
    thA.join();

    Thread thB = Thread(printChar, new ThreadContext("b", 25));
    Thread thF = Thread(printChar, new ThreadContext("f", 10));
    Thread thD = Thread(printChar, new ThreadContext("d", 10));

    Thread thC = Thread(printCharMutexed, new MutexedThreadContext("c", 5, &mutex));
    thC.join();
    Thread thE = Thread(printCharMutexed, new MutexedThreadContext("e", 5, &mutex));

    thE.join();
    thF.join();
    thD.join();


    Thread thK = Thread(printChar, new ThreadContext("k", 10));
    Thread thM = Thread(printChar, new ThreadContext("m", 10));
    Thread thG = Thread(printCharSemaphored, new SemaphoredThreadContext("g", 5, switches));
    thG.join();
    Thread thH = Thread(printCharSemaphored, new SemaphoredThreadContext("h", 5, switches));

    thK.join();
    thM.join();
    thH.join();

    Thread thN = Thread(printChar, new ThreadContext("n", 5));

    thN.join();
    thB.join();

    Thread thP = Thread(printChar, new ThreadContext("p", 5));
    thP.join();

    return 0;
}

#ifdef __GNUC__

void *
#elif defined _WIN32 || defined _WIN64
DWORD WINAPI
#endif
printChar(void *arg) {
    ThreadContext *ctx = (ThreadContext *) arg;
    for (int i = 0; i < ctx->length; i++) {
        std::cout << ctx->ch;
        std::cout.flush();
        usleep(100000);
    };
    std::cout << std::endl;
    return NULL;
}

#ifdef __GNUC__

void *
#elif defined _WIN32 || defined _WIN64
DWORD WINAPI
#endif
printCharMutexed(void *arg) {
    MutexedThreadContext *ctx = (MutexedThreadContext *) arg;
    ctx->mutex->lock();
    printChar(ctx);
    ctx->mutex->unlock();
    return NULL;
};

#ifdef __GNUC__

void *
#elif defined _WIN32 || defined _WIN64
DWORD WINAPI
#endif
printCharSemaphored(void *arg) {
    SemaphoredThreadContext *ctx = (SemaphoredThreadContext *) arg;
    for (int i = 0; i < ctx->length; i++) {
        for (unsigned int j = 0; j < ctx->switches.size(); j++) {
            if (ctx->ch == ctx->switches[j].ch) {
                ctx->switches[j].semWait->wait();
                std::cout << ctx->ch;
                std::cout.flush();
                usleep(100000);
                ctx->switches[j].semPost->post();
                break;
            }
        }
    };
    std::cout << std::endl;
    return NULL;
}
