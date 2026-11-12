#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/wait.h>	/* waitpid() for the 'p' command */
#include <pthread.h>	/* pthreads for the 't' command */

#include "scull.h"

#define CDEV_NAME "/dev/scull"

/* Number of child processes ('p') or threads ('t') to create */
#define NUM_WORKERS 4

/* Quantum command line option */
static int g_quantum;

static void usage(const char *cmd)
{
	printf("Usage: %s <command>\n"
	       "Commands:\n"
	       "  R          Reset quantum\n"
	       "  S <int>    Set quantum\n"
	       "  T <int>    Tell quantum\n"
	       "  G          Get quantum\n"
	       "  Q          Query quantum\n"
	       "  X <int>    Exchange quantum\n"
	       "  H <int>    Shift quantum\n"
	       "  i          Get info about the current task\n"
	       "  p          Get task info from 4 child processes\n"
	       "  t          Get task info from 4 threads\n"
	       "  h          Print this message\n",
	       cmd);
}

typedef int cmd_t;

static cmd_t parse_arguments(int argc, const char **argv)
{
	cmd_t cmd;

	if (argc < 2) {
		fprintf(stderr, "%s: Invalid number of arguments\n", argv[0]);
		cmd = -1;
		goto ret;
	}

	/* Parse command and optional int argument */
	cmd = argv[1][0];
	switch (cmd) {
	case 'S':
	case 'T':
	case 'H':
	case 'X':
		if (argc < 3) {
			fprintf(stderr, "%s: Missing quantum\n", argv[0]);
			cmd = -1;
			break;
		}
		g_quantum = atoi(argv[2]);
		break;
	case 'R':
	case 'G':
	case 'Q':
	case 'i':
	case 'p':
	case 't':
	case 'h':
		break;
	default:
		fprintf(stderr, "%s: Invalid command\n", argv[0]);
		cmd = -1;
	}

ret:
	if (cmd < 0 || cmd == 'h') {
		usage(argv[0]);
		exit((cmd == 'h')? EXIT_SUCCESS : EXIT_FAILURE);
	}
	return cmd;
}

/*
 * Issue SCULL_IOCIQUANTUM and print the returned task info on one line.
 * Returns the ioctl result (0 on success).
 */
static int do_info(int fd)
{
	struct task_info info;
	int ret;

	ret = ioctl(fd, SCULL_IOCIQUANTUM, &info);
	if (ret == 0)
		printf("state %u, cpu %u, prio %d, pid %d, tgid %d, nv %lu, niv %lu\n",
		       info.__state, info.cpu, info.prio, info.pid, info.tgid,
		       info.nvcsw, info.nivcsw);
	return ret;
}

/*
 * Work done by each child process / thread: use the info ioctl twice,
 * reporting any failure. Returns 0 if both ioctls succeeded.
 */
static int do_info_twice(int fd)
{
	int i, ret = 0;

	for (i = 0; i < 2; i++) {
		if (do_info(fd) != 0) {
			perror("ioctl");
			ret = -1;
		}
	}
	return ret;
}

/*
 * 'p' command: create NUM_WORKERS child processes that run concurrently.
 * All children are forked before the parent waits for any of them. The
 * children inherit the already-open fd, so the device is opened and closed
 * only once (by main in the parent).
 */
static int do_processes(int fd)
{
	pid_t pids[NUM_WORKERS];
	int i, n = 0, status, ret = 0;

	/* Flush pending output so children don't re-print the parent's buffer */
	fflush(stdout);

	for (i = 0; i < NUM_WORKERS; i++) {
		pid_t pid = fork();

		if (pid < 0) {
			perror("fork");
			ret = -1;
			break;
		}
		if (pid == 0) {
			/* Child: two ioctls, then exit without returning to main
			 * (so it doesn't close the device or print "closed") */
			int r = do_info_twice(fd);

			fflush(stdout);
			_exit((r == 0) ? EXIT_SUCCESS : EXIT_FAILURE);
		}
		pids[n++] = pid;	/* Parent: remember the child */
	}

	/* Parent: wait for every child that was created */
	for (i = 0; i < n; i++) {
		if (waitpid(pids[i], &status, 0) < 0) {
			perror("waitpid");
			ret = -1;
		} else if (!WIFEXITED(status) || WEXITSTATUS(status) != EXIT_SUCCESS) {
			ret = -1;
		}
	}
	return ret;
}

/* Thread body for 't': arg is the address of the open fd */
static void *thread_fn(void *arg)
{
	int fd = *(int *)arg;

	do_info_twice(fd);
	return NULL;
}

/*
 * 't' command: create NUM_WORKERS threads that run concurrently. All
 * threads are created before the main thread joins any of them. They share
 * the fd opened once by main.
 */
static int do_threads(int *fd)
{
	pthread_t threads[NUM_WORKERS];
	int i, n = 0, err, ret = 0;

	for (i = 0; i < NUM_WORKERS; i++) {
		err = pthread_create(&threads[n], NULL, thread_fn, fd);
		if (err != 0) {
			fprintf(stderr, "pthread_create failed (error %d)\n", err);
			ret = -1;
			break;
		}
		n++;
	}

	/* Wait for every thread that was created */
	for (i = 0; i < n; i++)
		pthread_join(threads[i], NULL);
	return ret;
}

static int do_op(int fd, cmd_t cmd)
{
	int ret, q;

	switch (cmd) {
	case 'R':
		ret = ioctl(fd, SCULL_IOCRESET);
		if (ret == 0)
			printf("Quantum reset\n");
		break; case 'Q':
		q = ioctl(fd, SCULL_IOCQQUANTUM);
		printf("Quantum: %d\n", q);
		ret = 0;
		break;
	case 'G':
		ret = ioctl(fd, SCULL_IOCGQUANTUM, &q);
		if (ret == 0)
			printf("Quantum: %d\n", q);
		break;
	case 'T':
		ret = ioctl(fd, SCULL_IOCTQUANTUM, g_quantum);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'S':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCSQUANTUM, &q);
		if (ret == 0)
			printf("Quantum set\n");
		break;
	case 'X':
		q = g_quantum;
		ret = ioctl(fd, SCULL_IOCXQUANTUM, &q);
		if (ret == 0)
			printf("Quantum exchanged, old quantum: %d\n", q);
		break;
	case 'H':
		q = ioctl(fd, SCULL_IOCHQUANTUM, g_quantum);
		printf("Quantum shifted, old quantum: %d\n", q);
		ret = 0;
		break;
	case 'i':
		ret = do_info(fd);
		break;
	case 'p':
		/* Errors are already reported inside, so skip perror below */
		return do_processes(fd);
	case 't':
		/* Pass the address of fd to the threads, as the hint suggests */
		return do_threads(&fd);
	default:
		/* Should never occur */
		abort();
		ret = -1; /* Keep the compiler happy */
	}

	if (ret != 0)
		perror("ioctl");
	return ret;
}

int main(int argc, const char **argv)
{
	int fd, ret;
	cmd_t cmd;

	cmd = parse_arguments(argc, argv);

	fd = open(CDEV_NAME, O_RDONLY);
	if (fd < 0) {
		perror("cdev open");
		return EXIT_FAILURE;
	}

	printf("Device (%s) opened\n", CDEV_NAME);

	ret = do_op(fd, cmd);

	if (close(fd) != 0) {
		perror("cdev close");
		return EXIT_FAILURE;
	}

	printf("Device (%s) closed\n", CDEV_NAME);

	return (ret != 0)? EXIT_FAILURE : EXIT_SUCCESS;
}
