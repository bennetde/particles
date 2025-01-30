#include "deletion_queue.h"

void DeletionQueue::pushFunction(std::function<void()>&& function) {
    deletors.push_back(function);
}

void DeletionQueue::flush() {
    for (auto it = deletors.rbegin(); it != deletors.rend(); it++) {
        (*it)();
    }

    deletors.clear();
}