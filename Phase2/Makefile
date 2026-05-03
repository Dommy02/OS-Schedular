build:
	gcc process_generator.c -o process_generator.out
	gcc clk.c -o clk.out
	g++ scheduler.cpp -o scheduler.out
	gcc process.c -o process.out
	gcc test_generator.c -o test_generator.out
	gcc process_generatorRR.c -o process_generatorRR.out
	gcc schedulerRR.c utils.c -o schedulerRR.out -lm
	gcc processRR.c utils.c -o processRR.out
clean:
	rm -f *.out  processes.txt

all: clean build

run:
	./process_generator.out