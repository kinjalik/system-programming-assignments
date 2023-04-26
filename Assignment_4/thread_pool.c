#include "thread_pool.h"
#include <stdlib.h>
#include <pthread.h>
#include <errno.h>
#include <stdio.h>
#include <tgmath.h>
#include <assert.h>
#include <sys/time.h>
#include <stdint.h>

struct thread_pool {
	int max_thread_count;
	int task_waiting;
    int task_executing;
	pthread_t *threads;
    int created_threads_count;
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool shutdown;

    struct thread_task *queue[TPOOL_MAX_TASKS];
    size_t writePtr;
    size_t readPtr;
};

enum task_state {
    TASK_CREATED,
    TASK_QUEUED,
    TASK_EXECUTING,
    TASK_DONE,
    TASK_JOINED
};

struct thread_task {
	thread_task_f function;
	void *arg;
	void *result;
    pthread_mutex_t mutex;
    pthread_cond_t cond;
    enum task_state state;
    bool detachMode;
};

static void *thread_worker(void *arg) {
	struct thread_pool *pool = (struct thread_pool *)arg;
	while (true) {
        pthread_mutex_lock(&pool->mutex);
        while (pool->task_waiting == 0 && !pool->shutdown) {
            pthread_cond_wait(&pool->cond, &pool->mutex);
        }

        if (pool->shutdown) {
            pthread_mutex_unlock(&pool->mutex);
            break;
        }

        struct thread_task *task = pool->queue[pool->readPtr];
        pool->queue[pool->readPtr] = NULL;
        pool->readPtr++;
        pool->task_waiting--;

        if (pool->readPtr == TPOOL_MAX_TASKS) {
            pool->readPtr = 0;
        }

        pool->task_executing++;
        pthread_mutex_unlock(&pool->mutex);
        if (task == NULL) {
            pool->task_executing--;
            continue;
        }

        pthread_mutex_lock(&task->mutex);
        task->state = TASK_EXECUTING;
        pthread_mutex_unlock(&task->mutex);
        task->result = task->function(task->arg);


        pthread_mutex_lock(&pool->mutex);
        pool->task_executing--;
        pthread_mutex_unlock(&pool->mutex);

        pthread_mutex_lock(&task->mutex);
        task->state = TASK_DONE;

        pthread_cond_broadcast(&task->cond);

        bool to_delete = task->detachMode;
        pthread_mutex_unlock(&task->mutex);
        if (to_delete) {
            pthread_mutex_destroy(&task->mutex);
            pthread_cond_destroy(&task->cond);
            free(task);
        }
	}
	return NULL;
}

int thread_pool_new(int max_thread_count, struct thread_pool **pool) {
	if (max_thread_count <= 0 || max_thread_count > TPOOL_MAX_THREADS || pool == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	struct thread_pool *p = calloc(1, sizeof(struct thread_pool));
	if (p == NULL) {
		return TPOOL_ERR_ALLOCATION_ERROR;
	}
	p->max_thread_count = max_thread_count;
	p->task_waiting  = 0;
    p->task_executing = 0;
	p->threads = calloc(max_thread_count, sizeof(pthread_t));
	if (p->threads == NULL) {
		free(p);
		return TPOOL_ERR_ALLOCATION_ERROR;
	}
    p->created_threads_count = 0;
	pthread_mutex_init(&p->mutex, NULL);
	pthread_cond_init(&p->cond, NULL);
	p->shutdown = false;

	/*
	for (int i = 0; i < max_thread_count; i++) {
		if (pthread_create(&p->threads[i], NULL, thread_worker, p) != 0) {
			thread_pool_delete(p);
			return errno;
		}
	}
	*/

	*pool = p;
	return TPOOL_OK;
}

int thread_pool_thread_count(const struct thread_pool *pool) {
	if (pool == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	return pool->created_threads_count;
}

int thread_pool_delete(struct thread_pool *pool) {
	if (pool == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	pthread_mutex_lock(&pool->mutex);

    if (pool->task_executing > 0 || pool->task_waiting > 0) {
        pthread_mutex_unlock(&pool->mutex);
        return TPOOL_ERR_HAS_TASKS;
    }

	pool->shutdown = true;
	pthread_cond_broadcast(&pool->cond);
	pthread_mutex_unlock(&pool->mutex);

	for (int i = 0; i < pool->created_threads_count; i++) {
		pthread_join(pool->threads[i], NULL);
	}

	free(pool->threads);
	pthread_mutex_destroy(&pool->mutex);
	pthread_cond_destroy(&pool->cond);
	free(pool);

	return TPOOL_OK;
}

int thread_pool_push_task(struct thread_pool *pool, struct thread_task *task) {
	if (pool == NULL || task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

    pthread_mutex_lock(&pool->mutex);
	if (pool->task_waiting + pool->task_executing >= TPOOL_MAX_TASKS) {
        pthread_mutex_unlock(&pool->mutex);
		return TPOOL_ERR_TOO_MANY_TASKS;
	}

    pthread_mutex_lock(&task->mutex);
    task->state = TASK_QUEUED;
    pthread_mutex_unlock(&task->mutex);
    pool->queue[pool->writePtr] = task;
    pool->writePtr++;
    pool->task_waiting++;
    if (pool->writePtr == TPOOL_MAX_TASKS) pool->writePtr = 0;

    if (pool->task_executing < pool->created_threads_count || pool->created_threads_count >= pool->max_thread_count) {
        pthread_cond_signal(&pool->cond);
    } else {
        int ret = pthread_create(&pool->threads[pool->created_threads_count], NULL, thread_worker, pool);
        if (ret != 0) {
            return TPOOL_ERR_PTHREAD_ERROR;
        }
        pool->created_threads_count++;
    }
    pthread_mutex_unlock(&pool->mutex);
	return TPOOL_OK;
}

int thread_task_new(struct thread_task **task, thread_task_f function, void *arg) {
	if (task == NULL || function == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}
	struct thread_task *t = calloc(1, sizeof(struct thread_task));
	if (t == NULL) {
		return TPOOL_ERR_ALLOCATION_ERROR;
	}
	t->function = function;
	t->arg = arg;
    pthread_mutex_init(&t->mutex, NULL);
    pthread_cond_init(&t->cond, NULL);

    *task = t;

    return TPOOL_OK;

}

int thread_task_timed_join(struct thread_task *task, double timeout, void **result) {
	if (task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

    pthread_mutex_lock(&task->mutex);
	if (task->state == TASK_CREATED) {
        pthread_mutex_unlock(&task->mutex);
		return TPOOL_ERR_TASK_NOT_PUSHED;
	}

    if (task->state == TASK_DONE || task->state == TASK_JOINED) {
        task->state = TASK_JOINED;
        *result = task->result;
        pthread_mutex_unlock(&task->mutex);
        return TPOOL_OK;
    }

    if (timeout < 10e-7) {
        goto timedout;
    }
    timeout += 10e-7;

    struct timeval now;
    gettimeofday(&now, NULL);

    struct timespec deadline;
    deadline.tv_sec = now.tv_sec + (uint64_t) timeout;
    deadline.tv_nsec = now.tv_usec * 1000 + (timeout - (double) (uint64_t) timeout) * 1000000000;
    if (deadline.tv_nsec > 1000000000) {
        deadline.tv_sec++;
        deadline.tv_nsec -= 1000000000;
    }
    bool finished = false;
    do {
        int ret = pthread_cond_timedwait(&task->cond, &task->mutex, &deadline);
        finished = ret == ETIMEDOUT || task->state == TASK_DONE || task->state == TASK_JOINED;
    } while (!finished);

    timedout:
    if (task->state != TASK_DONE && task->state != TASK_JOINED) {
        pthread_mutex_unlock(&task->mutex);
        return TPOOL_ERR_TIMEOUT;
    }

    task->state = TASK_JOINED;
    pthread_mutex_unlock(&task->mutex);
	if (result != NULL) {
		*result = task->result;
	}

	return TPOOL_OK;
}
int thread_task_join(struct thread_task *task, void **result) {
    if (task == NULL) {
        return TPOOL_ERR_INVALID_ARGUMENT;
    }

    pthread_mutex_lock(&task->mutex);
    if (task->state == TASK_CREATED) {
        pthread_mutex_unlock(&task->mutex);
        return TPOOL_ERR_TASK_NOT_PUSHED;
    }
    while (task->state != TASK_DONE && task->state != TASK_JOINED) {
        pthread_cond_wait(&task->cond, &task->mutex);
    }

    task->state = TASK_JOINED;
    if (result != NULL) {
        *result = task->result;
    }

    pthread_mutex_unlock(&task->mutex);
    return TPOOL_OK;
}

int thread_task_delete(struct thread_task *task) {
	if (task == NULL) {
		return TPOOL_ERR_INVALID_ARGUMENT;
	}

    pthread_mutex_lock(&task->mutex);

    if (task->state == TASK_CREATED || task->state == TASK_JOINED || (task->detachMode && task->state == TASK_DONE)) {
        pthread_mutex_unlock(&task->mutex);
        pthread_mutex_destroy(&task->mutex);
        pthread_cond_destroy(&task->cond);
        free(task);
        return TPOOL_OK;
    }

    pthread_mutex_unlock(&task->mutex);
    return TPOOL_ERR_TASK_IN_POOL;
}

int thread_task_detach(struct thread_task *task) {
    pthread_mutex_lock(&task->mutex);
    if (task->state == TASK_CREATED) {
        pthread_mutex_unlock(&task->mutex);
        return TPOOL_ERR_TASK_NOT_PUSHED;
    }

    if (task->state == TASK_DONE) {
        pthread_mutex_unlock(&task->mutex);
        pthread_mutex_destroy(&task->mutex);
        pthread_cond_destroy(&task->cond);
        return TPOOL_OK;
    }

    task->detachMode = true;
    pthread_mutex_unlock(&task->mutex);
    return TPOOL_OK;
}
