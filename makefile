
dead_sample: dead_lock_stub.h  main.cpp 
	g++ -g -std=c++11  main.cpp -lpthread -ldw -ldl   -o dead_sample

