#
# Makefile para programas de teste
#

CC=gcc
LIB_DIR=../lib

all: priority_queue ccreate cyield cjoin csemaphore

priority_queue: priority_queue_test.c
	$(CC) -o priority_queue_test priority_queue_test.c -Wall -L$(LIB_DIR) -lcthread -Wall

ccreate: ccreate_test.c
	$(CC) -o ccreate_test ccreate_test.c -Wall -L$(LIB_DIR) -lcthread -Wall

cyield: cyield_test.c
	$(CC) -o cyield_test cyield_test.c -Wall -L$(LIB_DIR) -lcthread -Wall

cjoin: cjoin_test.c
	$(CC) -o cjoin_test cjoin_test.c -Wall -L$(LIB_DIR) -lcthread -Wall

csemaphore: csemaphore_test.c
	$(CC) -o csemaphore_test csemaphore_test.c -Wall -L$(LIB_DIR) -lcthread -Wall

clean:
	rm -rf csemaphore_test priority_queue_test ccreate_test cyield_test cjoin_test *.log *.o *~