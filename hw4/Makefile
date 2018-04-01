all: hw4
hw4: hw4.c
	cc hw4.c -l pthread -o hw4
.PHONY:run
run:hw4
	./hw4 -data ../data -output ./submission.csv -tree 20 -thread 20
clean:
	rm hw4


