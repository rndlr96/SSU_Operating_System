
TARGETS := sched

SCHED_OBJS := sched.o

OBJS := $(SCHED_OBJS)

CC := gcc

CFLAGS += -D_REENTRANT -D_LIBC_REENTRANT -D_THREAD_SAFE
CFLAGS += -Wall
CFLAGS += -Wunused
CFLAGS += -Wshadow
CFLAGS += -Wdeclaration-after-statement
CFLAGS += -Wdisabled-optimization
CFLAGS += -Wpointer-arith
CFLAGS += -Wredundant-decls
CFLAGS += -g -O2

LDFLAGS +=

%.o: %.c
	$(CC) -o $*.o $< -c $(CFLAGS)

.PHONY: all clean test

all: $(TARGETS)

clean:
	-rm -f $(TARGETS) $(OBJS) *~ *.bak core*

test: $(TARGETS)
	./sched data1.txt > result1.txt

	#프로세스 실행결과를 txt에 저장하고 싶다면 &>가 아닌 >를 사용
	#오류 메시지는 화면에 출력되고 실행결과만 저장된다.
	#&>로 하게 되면 오류 메세지만 txt에 저장되고 프로세스 실행
	#과정만이 출력된다.

sched: $(SCHED_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)
