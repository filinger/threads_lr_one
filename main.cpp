#include <iostream>
#include <vector>

class Thread {
public:
	virtual ~Thread() {}
	virtual void join() = 0;
	virtual void close() = 0;
};

class Mutex {
public:
	virtual ~Mutex() {}
	virtual void lock() = 0;
	virtual void unlock() = 0;
};

class Semaphore {
public:
	virtual ~Semaphore() {}
	virtual void wait() = 0;
	virtual void post() = 0;
};

#ifdef __GNUC__
#include "pthread.h"
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
		pthread_mutex_init(mt, NULL);
	}

	~PosixMutex() {
		pthread_mutex_destroy(mt);
	}

	void lock() override {
		pthread_mutex_lock(mt);
	}

	void unlock() override {
		pthread_mutex_unlock(mt);
	}

private:
	pthread_mutex_t* mt;
};

class PosixSemaphore : public Semaphore {
public:
	PosixSemaphore(const char* name, unsigned int initial, unsigned int max) {
		sm = sem_open(name, initial, max);
	}
	
	~PosixSemaphore() {
		sem_close(sm);
	}

	void wait()	override {
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
	WinSemaphore(const wchar_t* name, unsigned int initial, unsigned int max) {
		sm = CreateSemaphore(NULL, initial, max, name);
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
#else
#error "unknown platform"
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


Mutex* getMutex();
Semaphore* getSemaphore(const char* name, unsigned int initial, unsigned int max);

#ifdef __GNUC__
Thread* getThread(void *(procedure)(void *), void *ctx);
void *printChar(void *arg);
void *printCharMutexed(void *arg);
void *printCharSemaphored(void *arg);
#elif defined _WIN32 || defined _WIN64
Thread* getThread(LPTHREAD_START_ROUTINE routine, void *ctx);
DWORD WINAPI printChar(LPVOID arg);
DWORD WINAPI printCharMutexed(LPVOID arg);
DWORD WINAPI printCharSemaphored(LPVOID arg);
#endif

int main() {
	Mutex* mutex = getMutex();
	Semaphore* smK = getSemaphore("smK", 0, 0);
	Semaphore* smM = getSemaphore("smM", 0, 1);

    std::vector<SemaphoreSwitch> switches = {
            {"k", smK, smM},
            {"m", smM, smK}
    };

    Thread* thA = getThread(printChar, new ThreadContext("a"));
	thA->join();

	Thread* thB = getThread(printChar, new ThreadContext("b"));

	Thread* thF = getThread(printCharMutexed, new MutexedThreadContext("f", mutex));
	Thread* thD = getThread(printCharMutexed, new MutexedThreadContext("d", mutex));

	Thread* thC = getThread(printChar, new ThreadContext("c"));
    thC->join();
	Thread* thE = getThread(printChar, new ThreadContext("e"));

    thE->join();
    thF->join();
    thD->join();

	Thread* thK = getThread(printCharSemaphored, new SemaphoredThreadContext("k", switches));
	Thread* thM = getThread(printCharSemaphored, new SemaphoredThreadContext("m", switches));

	Thread* thG = getThread(printChar, new ThreadContext("g"));
    thG->join();
	Thread* thH = getThread(printChar, new ThreadContext("h"));

    thH->join();
    thK->join();
    thM->join();

	Thread* thN = getThread(printChar, new ThreadContext("n"));

    thN->join();
    thB->join();


	Thread* thP = getThread(printChar, new ThreadContext("p"));
    thP->join();

	return 0;
}


#ifdef __GNUC__
Thread* getThread(void *(procedure)(void *), void *ctx) {
	return new PosixThread(procedure, ctx);
}
#elif defined _WIN32 || defined _WIN64
Thread* getThread(LPTHREAD_START_ROUTINE routine, void *ctx) {
	return new WinThread(routine, ctx);
}
#endif


Mutex* getMutex()
{
#ifdef __GNUC__
	return new PosixMutex();
#elif defined _WIN32 || defined _WIN64
	return new WinMutex(L"win32");
#endif
}

Semaphore* getSemaphore(const char* name, unsigned int initial, unsigned int max)
{
#ifdef __GNUC__
	return new PosixSemaphore(name, initial, max);
#elif defined _WIN32 || defined _WIN64
	return new WinSemaphore(NULL, initial, max);
#endif
}

#ifdef __GNUC__

void *printChar(void *arg) {
	ThreadContext *ctx = (ThreadContext *)arg;
	for (int i = 0; i < 1000; i++) {
		std::cout << ctx->ch;
	};
	std::cout << std::endl;
	return NULL;
}

void *printCharMutexed(void *arg) {
	MutexedThreadContext *ctx = (MutexedThreadContext *)arg;
	ctx->mutex->lock();
	printChar(ctx);
	ctx->mutex->unlock();
	return NULL;
};

void *printCharSemaphored(void *arg) {
	SemaphoredThreadContext *ctx = (SemaphoredThreadContext *)arg;
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

#elif defined _WIN32 || defined _WIN64

DWORD WINAPI printChar(LPVOID arg) {
	ThreadContext *ctx = (ThreadContext *)arg;
	for (int i = 0; i < 1000; i++) {
		std::cout << ctx->ch;
	};
	std::cout << std::endl;
	return NULL;
}

DWORD WINAPI printCharMutexed(LPVOID arg) {
	MutexedThreadContext *ctx = (MutexedThreadContext *)arg;
	ctx->mutex->lock();
	printChar(ctx);
	ctx->mutex->unlock();
	return NULL;
};

DWORD WINAPI printCharSemaphored(LPVOID arg) {
	SemaphoredThreadContext *ctx = (SemaphoredThreadContext *)arg;
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

#endif
