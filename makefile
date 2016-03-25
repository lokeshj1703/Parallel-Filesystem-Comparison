openmp: openmp827.cpp
	g++ -fopenmp -o openmp openmp827.cpp -I/usr/include/hashlib++/ -lhl++
pthread: pthread827.cpp
	g++ -pthread -o pthread pthread827.cpp -I/usr/include/hashlib++/ -lhl++
