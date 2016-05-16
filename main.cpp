#include "pthread.h"
#include "semaphore.h"

#include <iostream>
#include <vector>

struct ThreadContext {
    ThreadContext(const char *ch) : ch(ch) { }

    const char *ch;
};

struct MutexedThreadContext : public ThreadContext {
    MutexedThreadContext(const char *ch, pthread_mutex_t *mutex) : ThreadContext(ch), mutex(mutex) { }

    pthread_mutex_t *mutex;
};

struct SemaphoreSwitch {
    const char *ch;
    sem_t *semWait;
    sem_t *semPost;
};

struct SemaphoredThreadContext : public ThreadContext {
    SemaphoredThreadContext(const char *ch, const std::vector<SemaphoreSwitch> &switches)
            : ThreadContext(ch), switches(switches) { }

    std::vector<SemaphoreSwitch> switches;
};

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
    pthread_mutex_lock(ctx->mutex);
    printChar(ctx);
    pthread_mutex_unlock(ctx->mutex);
    return NULL;
};

void *printCharSemaphored(void *arg) {
    SemaphoredThreadContext *ctx = (SemaphoredThreadContext *) arg;

    for (int i = 0; i < 1000; i++) {
        for (int j = 0; j < ctx->switches.size(); j++) {
            if (ctx->ch == ctx->switches[j].ch) {
                sem_wait(ctx->switches[j].semWait);
                std::cout << ctx->ch;
                sem_post(ctx->switches[j].semPost);
                break;
            }
        }

    };
    std::cout << std::endl;
    return NULL;
}

pthread_t startThread(void *(procedure)(void *), ThreadContext *ctx) {
    pthread_t th;
    pthread_create(&th, NULL, procedure, ctx);
    return th;
}

int main() {
    void *result;

    pthread_mutex_t mutex;
    pthread_mutex_init(&mutex, NULL);

    sem_t *smK = sem_open("smK", 0, 0);
    sem_t *smM = sem_open("smM", 0, 1);

    std::vector<SemaphoreSwitch> switches = {
            {"k", smK, smM},
            {"m", smM, smK}
    };

    pthread_t thA = startThread(printChar, new ThreadContext("a"));
    pthread_join(thA, &result);


    pthread_t thB = startThread(printChar, new ThreadContext("b"));

    pthread_t thF = startThread(printCharMutexed, new MutexedThreadContext("f", &mutex));
    pthread_t thD = startThread(printCharMutexed, new MutexedThreadContext("d", &mutex));

    pthread_t thC = startThread(printChar, new ThreadContext("c"));
    pthread_join(thC, &result);
    pthread_t thE = startThread(printChar, new ThreadContext("e"));

    pthread_join(thE, &result);
    pthread_join(thF, &result);
    pthread_join(thD, &result);

    pthread_t thK = startThread(printCharSemaphored, new SemaphoredThreadContext("k", switches));
    pthread_t thM = startThread(printCharSemaphored, new SemaphoredThreadContext("m", switches));

    pthread_t thG = startThread(printChar, new ThreadContext("g"));
    pthread_join(thG, &result);
    pthread_t thH = startThread(printChar, new ThreadContext("h"));

    pthread_join(thH, &result);
    pthread_join(thK, &result);
    pthread_join(thM, &result);

    pthread_t thN = startThread(printChar, new ThreadContext("n"));

    pthread_join(thN, &result);
    pthread_join(thB, &result);


    pthread_t thP = startThread(printChar, new ThreadContext("p"));
    pthread_join(thP, &result);


    pthread_mutex_destroy(&mutex);
    sem_close(smK);
    sem_close(smM);

    return 0;
}
