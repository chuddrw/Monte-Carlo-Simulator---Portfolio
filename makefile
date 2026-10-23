CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS = -lm
DEPS = distributions.h evaluate.h protocols.h

all: monte_carlo worker aggregator

monte_carlo: main.o distributions.o evaluate.o protocols.o
	$(CC) -o $@ $^ $(LDFLAGS)

worker: worker.o distributions.o evaluate.o protocols.o
	$(CC) -o $@ $^ $(LDFLAGS)

aggregator: aggregator.o distributions.o protocols.o
	$(CC) -o $@ $^ $(LDFLAGS)

main.o: main.c $(DEPS)
	$(CC) $(CFLAGS) -c main.c

worker.o: worker.c $(DEPS)
	$(CC) $(CFLAGS) -c worker.c

aggregator.o: aggregator.c $(DEPS)
	$(CC) $(CFLAGS) -c aggregator.c

distributions.o: distributions.c distributions.h
	$(CC) $(CFLAGS) -c distributions.c

evaluate.o: evaluate.c evaluate.h
	$(CC) $(CFLAGS) -c evaluate.c

protocols.o: protocols.c protocols.h distributions.h
	$(CC) $(CFLAGS) -c protocols.c

clean:
	rm -f *.o monte_carlo worker aggregator

# CC = gcc
# CFLAGS = -Wall -Wextra -g
# LDFLAGS = -lm
# DEPS = distributions.h evaluate.h

# all: monte_carlo

# monte_carlo: main.o worker.o distributions.o evaluate.o
# 	$(CC) -o $@ $^ $(LDFLAGS)

# %.o: %.c $(DEPS)
# 	$(CC) $(CFLAGS) -c $<
    
# clean:
# 	rm -f *.o monte_carlo