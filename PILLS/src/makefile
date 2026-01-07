CC := g++ -std=c++20
sol := 

main: main.o solve.o
	$(CC) -O3 -o $@ $^ -lboost_thread -lboost_system -lpthread

%.o: %.cpp
	$(CC) -c -O3 -o $@ $<

clean:
	find . -name "*.o" | xargs rm -rf
	rm -f main