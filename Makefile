all: master_test worker_test

master_test:
	g++ -o master_test master_test.cpp distributor/*.cpp -I. -lzmq -lglog -lboost_system -lboost_thread -lprotobuf
worker_test:
	g++ -o worker_test worker_test.cpp distributor/*.cpp -I. -lzmq -lglog -lboost_system -lboost_thread -lprotobuf
