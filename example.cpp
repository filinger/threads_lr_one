//
// Created by Maksim Sigida on 5/21/16.
//

#include <iostream>

pthread_mutex_t mt;

void *printCharacter(void *arg) {
    pthread_mutex_lock(&mt);

    for (int i = 0; i < 100; i++) {
        std::cout << (char *) arg;
    };
    std::cout << std::endl;

    pthread_mutex_unlock(&mt);
    return NULL;
}

int main_example() {
    void *result;

    pthread_mutex_init(&mt, NULL);

    pthread_t thB;
    pthread_create(&thB, NULL, printCharacter, (void *) "b");

    pthread_t thA;
    pthread_create(&thA, NULL, printCharacter, (void *) "a");

    pthread_join(thA, &result);
    pthread_mutex_destroy(&mt);
    return 0;
}