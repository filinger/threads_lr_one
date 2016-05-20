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

#else
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
	WinMutex(const wchar_t *name) : name(name) {
		mt = CreateMutex(NULL, false, name);
	}

	void lock() override {
		mt = OpenMutex(NULL, false, name);
	}

	void unlock() override {
		ReleaseMutex(mt);
	}

private:
	const wchar_t *name;
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

#ifdef __GNUC__
#define Thread PosixThread

void *printChar(void *arg);
void *printCharMutexed(void *arg);
void *printCharSemaphored(void *arg);

#else
#define Thread WinThread

DWORD WINAPI printChar(void *arg);
DWORD WINAPI printCharMutexed(void *arg);
DWORD WINAPI printCharSemaphored(void *arg);

#endif

int main() {
#ifdef __GNUC__
    PosixMutex mutex;
    PosixSemaphore smK("smK", 0, 0);
    PosixSemaphore smM("smM", 0, 1);
#else
    WinMutex mutex(L"mt");
    WinSemaphore smK(0, 0);
    WinSemaphore smM(0, 1);
#endif

    std::vector<SemaphoreSwitch> switches = {
            {"k", &smK, &smM},
            {"m", &smM, &smK}
    };

    Thread thA = Thread(printChar, new ThreadContext("a"));
    thA.join();

    Thread thB = Thread(printChar, new ThreadContext("b"));

    Thread thF = Thread(printCharMutexed, new MutexedThreadContext("f", &mutex));
    Thread thD = Thread(printCharMutexed, new MutexedThreadContext("d", &mutex));

    Thread thC = Thread(printChar, new ThreadContext("c"));
    thC.join();
    Thread thE = Thread(printChar, new ThreadContext("e"));

    thE.join();
    thF.join();
    thD.join();

    Thread thK = Thread(printCharSemaphored, new SemaphoredThreadContext("k", switches));
    Thread thM = Thread(printCharSemaphored, new SemaphoredThreadContext("m", switches));

    Thread thG = Thread(printChar, new ThreadContext("g"));
    thG.join();
    Thread thH = Thread(printChar, new ThreadContext("h"));

    thH.join();
    thK.join();
    thM.join();

    Thread thN = Thread(printChar, new ThreadContext("n"));

    thN.join();
    thB.join();


    Thread thP = Thread(printChar, new ThreadContext("p"));
    thP.join();

    return 0;
}

#ifdef __GNUC__
void *
#else
DWORD WINAPI
#endif
printChar(void *arg) {
    ThreadContext *ctx = (ThreadContext *) arg;
    for (int i = 0; i < 1000; i++) {
        std::cout << ctx->ch;
    };
    std::cout << std::endl;
    return NULL;
}

#ifdef __GNUC__
void *
#else
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
#else
DWORD WINAPI
#endif
printCharSemaphored(void *arg) {
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
