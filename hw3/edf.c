#include <stdio.h>
#include <stdlib.h>

struct task {
    int cpu;
    int period;
};

struct job {
    int pid;
    int left;
    int release;
    int deadline;
};

static int gcd(int a, int b) {
    while (b != 0) {
        int r = a % b;
        a = b;
        b = r;
    }
    return a;
}

static int lcm(int a, int b) {
    return a / gcd(a, b) * b;
}

static int job_before(const struct job *a, const struct job *b) {
    if (a->deadline != b->deadline) {
        return a->deadline < b->deadline;
    }
    if (a->release != b->release) {
        return a->release < b->release;
    }
    return a->pid < b->pid;
}

static void insert_job(struct job *queue, int *count, struct job job) {
    int i = *count;
    queue[i] = job;
    while (i > 0 && job_before(&queue[i], &queue[i - 1])) {
        struct job tmp = queue[i - 1];
        queue[i - 1] = queue[i];
        queue[i] = tmp;
        --i;
    }
    ++*count;
}

static void remove_first(struct job *queue, int *count) {
    for (int i = 1; i < *count; ++i) {
        queue[i - 1] = queue[i];
    }
    --*count;
}

static void print_queue(int t, const struct job *queue, int count) {
    printf("%d: processes:", t);
    for (int i = 0; i < count; ++i) {
        printf(" [%d|p=%d|r=%d|d=%d]", queue[i].pid, queue[i].left,
               queue[i].release, queue[i].deadline);
    }
    printf("\n");
}

int main(void) {
    int n;
    printf("Enter the number of processes to schedule: ");
    scanf("%d", &n);

    struct task *tasks = malloc(n * sizeof(struct task));
    for (int i = 0; i < n; ++i) {
        printf("Enter the CPU time of process %d: ", i + 1);
        scanf("%d", &tasks[i].cpu);
        printf("Enter the period of process %d: ", i + 1);
        scanf("%d", &tasks[i].period);
    }
    printf("\n");

    int max_time = 1;
    for (int i = 0; i < n; ++i) {
        max_time = lcm(max_time, tasks[i].period);
    }

    int total_jobs = 0;
    for (int i = 0; i < n; ++i) {
        total_jobs += max_time / tasks[i].period;
    }
    struct job *queue = malloc(total_jobs * sizeof(struct job));
    int count = 0;

    int running = 0;
    int run_pid = 0;
    int run_release = 0;
    int created = 0;
    int completed = 0;
    int waiting = 0;
    int max_lateness = 0;

    for (int t = 0; t <= max_time; ++t) {
        if (running && queue[0].left == 0) {
            printf("%d: process %d ends\n", t, queue[0].pid);
            ++completed;
            if (t - queue[0].deadline > max_lateness) {
                max_lateness = t - queue[0].deadline;
            }
            remove_first(queue, &count);
            running = 0;
        }

        if (t == max_time) {
            printf("%d: max time reached\n", t);
            print_queue(t, queue, count);
            break;
        }

        /* pid order to match the spec examples */
        for (int pid = 1; pid <= n; ++pid) {
            for (int i = 0; i < count; ++i) {
                if (queue[i].pid == pid && queue[i].deadline == t &&
                    queue[i].left > 0) {
                    printf("%d: process %d missed deadline (%d ms left)\n", t,
                           pid, queue[i].left);
                }
            }
        }

        int released = 0;
        for (int i = 0; i < n; ++i) {
            if (t % tasks[i].period == 0) {
                struct job job = {i + 1, tasks[i].cpu, t, t + tasks[i].period};
                insert_job(queue, &count, job);
                ++created;
                released = 1;
            }
        }
        if (released) {
            print_queue(t, queue, count);
        }

        if (count != 0 && (!running || queue[0].pid != run_pid ||
                           queue[0].release != run_release)) {
            if (running) {
                printf("%d: process %d preempted!\n", t, run_pid);
            }
            printf("%d: process %d starts\n", t, queue[0].pid);
            running = 1;
            run_pid = queue[0].pid;
            run_release = queue[0].release;
        }

        if (count != 0) {
            waiting += count - 1;
            --queue[0].left;
        }
    }

    printf("Number of processes created: %d\n", created);
    printf("Total waiting time: %d\n", waiting);
    printf("Average waiting time: %.2lf\n", (double)waiting / created);
    printf("Number of processes completed: %d\n", completed);
    printf("Maximum lateness: %d\n", max_lateness);

    free(tasks);
    free(queue);
    return 0;
}
