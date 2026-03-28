#include <iostream>
#include <atomic>
#include <vector>
#include <thread>
#include <chrono>
#include <random>
#include <cstdlib>

using namespace std;

// =========================
// Peterson Lock
// =========================
class PetersonLock {
public:
    atomic<bool> flag[2];
    atomic<int> victim;

    PetersonLock() {
        flag[0] = false;
        flag[1] = false;
        victim = 0;
    }

    void lock(int i) {
        flag[i] = true;
        victim = i;
        while (flag[1 - i] && victim == i) {}
    }

    void unlock(int i) {
        flag[i] = false;
    }
};

// =========================
// Tournament Tree Lock
// =========================
class TTLock {
public:
    int n;
    int size;
    vector<PetersonLock> tree;

    TTLock(int num_threads) {
        n = num_threads;

        size = 1;
        while (size < n) size *= 2;

        tree = vector<PetersonLock>(2 * size);
    }

    void acquire(int thread_id, int node, int left, int right) {
        if (right - left == 1) return;

        int mid = (left + right) / 2;
        int side;

        if (thread_id < mid) {
            side = 0;
            acquire(thread_id, node * 2, left, mid);
        } else {
            side = 1;
            acquire(thread_id, node * 2 + 1, mid, right);
        }

        tree[node].lock(side);
    }

    void release(int thread_id, int node, int left, int right) {
        if (right - left == 1) return;

        int mid = (left + right) / 2;
        int side;

        if (thread_id < mid) {
            side = 0;
            tree[node].unlock(side);
            release(thread_id, node * 2, left, mid);
        } else {
            side = 1;
            tree[node].unlock(side);
            release(thread_id, node * 2 + 1, mid, right);
        }
    }

    void lock(int thread_id) {
        acquire(thread_id, 1, 0, size);
    }

    void unlock(int thread_id) {
        release(thread_id, 1, 0, size);
    }
};

// =========================
// random sleep（1~500 ms）
// =========================
void random_sleep() {
    static thread_local mt19937 generator(random_device{}());
    uniform_int_distribution<int> dist(1, 500);
    this_thread::sleep_for(chrono::milliseconds(dist(generator)));
}

// =========================
// thinking / eating
// =========================
void thinking(int i) {
    cout << "Philosopher " << i << " starts thinking\n";
    random_sleep();
    cout << "Philosopher " << i << " ends thinking\n";
}

void eating(int i) {
    cout << "Philosopher " << i << " starts eating\n";
    random_sleep();
    cout << "Philosopher " << i << " ends eating\n";
}

// =========================
// Coarse-Grained
// =========================
void philosopher_coarse(int i, TTLock &bigLock) {
    for (int k = 0; k < 5; k++) {
        thinking(i);

        bigLock.lock(i);
        eating(i);
        bigLock.unlock(i);
    }
}

// =========================
// Fine-Grained
// =========================
void philosopher_fine(int i, int n, vector<PetersonLock> &chopsticks) {
    int left = i;
    int right = (i + 1) % n;

    for (int k = 0; k < 5; k++) {
        thinking(i);

        
        int self_left = 0;
        int self_right = 0;

        // left chopsticks：philosopher is i and (i-1+n)%n
        if (i < (i - 1 + n) % n) self_left = 0;
        else self_left = 1;

        // right chopsticks：philosopher is i and (i+1)%n
        if (i < (i + 1) % n) self_right = 0;
        else self_right = 1;

        // 防止死锁（奇偶策略）
        if (i % 2 == 0) {
            chopsticks[left].lock(self_left);
            chopsticks[right].lock(self_right);
        } else {
            chopsticks[right].lock(self_right);
            chopsticks[left].lock(self_left);
        }

        eating(i);

        chopsticks[left].unlock(self_left);
        chopsticks[right].unlock(self_right);
    }
}

// =========================
// main
// =========================
int main(int argc, char* argv[]) {

    if (argc < 2) {
        cout << "Usage: ./program n\n";
        return 1;
    }

    int n = atoi(argv[1]);

    cout << "===== Coarse-Grained =====\n";

    TTLock bigLock(n);
    vector<thread> threads1;

    for (int i = 0; i < n; i++) {
        threads1.emplace_back(philosopher_coarse, i, ref(bigLock));
    }

    for (auto &t : threads1) {
        t.join();
    }

    cout << "===== Fine-Grained =====\n";

    vector<PetersonLock> chopsticks(n);
    vector<thread> threads2;

    for (int i = 0; i < n; i++) {
        threads2.emplace_back(philosopher_fine, i, n, ref(chopsticks));
    }

    for (auto &t : threads2) {
        t.join();
    }

    return 0;
}