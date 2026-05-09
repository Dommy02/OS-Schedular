build:
	gcc clk.c -o clk.out
	gcc process_generatorRR.c utils.c -o process_generatorRR.out
	gcc schedulerRR.c utils.c -o schedulerRR.out -lm
	gcc processRR.c utils.c -o processRR.out
clean:
	rm -f *.out  processes.txt

all: clean build

run:
	./process_generatorRR.out 1	100