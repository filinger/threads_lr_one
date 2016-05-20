#include "pthread.h"
#include "semaphore.h"

#include <iostream>
#include <vector>

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

struct ThreadContext {
    ThreadContext(const char *ch) : ch(ch) { }

    const char *ch;
};

struct MutexedThreadContext : public ThreadContext {
    MutexedThreadContext(const char *ch, Mutex *mutex) : ThreadContext(ch), mutex(mutex) { }

    Mutex *mutex;
};

struct SemaphoreSwitch {
    const char *ch;
    Semaphore *semWait;
    Semaphore *semPost;
};

struct SemaphoredThreadContext : public ThreadContext {
    SemaphoredThreadContext(const char *ch, const std::vector<SemaphoreSwitch> &switches)
            : ThreadContext(ch), switches(switches) { }

    std::vector<SemaphoreSwitch> switches;
};

void *printChar(void *arg);

void *printCharMutexed(void *arg);

void *printCharSemaphored(void *arg);

int main() {
    PosixMutex mutex;
    PosixSemaphore smK("smK", 0, 0);
    PosixSemaphore smM("smM", 0, 1);
    std::vector<SemaphoreSwitch> switches = {
            {"k", &smK, &smM},
            {"m", &smM, &smK}
    };

    PosixThread thA = PosixThread(printChar, new ThreadContext("a"));
    thA.join();

    PosixThread thB = PosixThread(printChar, new ThreadContext("b"));

    PosixThread thF = PosixThread(printCharMutexed, new MutexedThreadContext("f", &mutex));
    PosixThread thD = PosixThread(printCharMutexed, new MutexedThreadContext("d", &mutex));

    PosixThread thC = PosixThread(printChar, new ThreadContext("c"));
    thC.join();
    PosixThread thE = PosixThread(printChar, new ThreadContext("e"));

    thE.join();
    thF.join();
    thD.join();

    PosixThread thK = PosixThread(printCharSemaphored, new SemaphoredThreadContext("k", switches));
    PosixThread thM = PosixThread(printCharSemaphored, new SemaphoredThreadContext("m", switches));

    PosixThread thG = PosixThread(printChar, new ThreadContext("g"));
    thG.join();
    PosixThread thH = PosixThread(printChar, new ThreadContext("h"));

    thH.join();
    thK.join();
    thM.join();

    PosixThread thN = PosixThread(printChar, new ThreadContext("n"));

    thN.join();
    thB.join();


    PosixThread thP = PosixThread(printChar, new ThreadContext("p"));
    thP.join();

    return 0;
}

void *printChar(void *arg) {
    ThreadContext *ctx = (ThreadContext *) arg;
    for (int i = 0; i < 1000; i++) {
        std::cout << ctx->ch;
    };
    std::cout << std::endl;
    return NULL;
}

void *printCharMutexed(void *arg) {
    MutexedThreadContext *ctx = (MutexedThreadContext *) arg;
    ctx->mutex->lock();
    printChar(ctx);
    ctx->mutex->unlock();
    return NULL;
};

void *printCharSemaphored(void *arg) {
    SemaphoredThreadContext *ctx = (SemaphoredThreadContext *) arg;
    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < ctx->switches.size(); j++) {
            if (ctx->ch == ctx->switches[j].ch) {
                ctx->switches[j].semWait->wait();
                std::cout << ctx->ch;
                ctx->switches[j].semPost->post();
                break;
            }
        }
    };
    std::cout << std::endl;
    return NULL;
}
