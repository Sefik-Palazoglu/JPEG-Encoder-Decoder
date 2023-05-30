all:
	mkdir -p bin
	g++ -std=c++11 -o bin/decoder.out src/decoder.cpp

clean:
	rm bin/decoder.out