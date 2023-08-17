all: client-phase1 client-phase2 client-phase3 client-phase4 client-phase5

client-phase1: client-phase1.cpp
	g++ -o client-phase1 -pthread client-phase1.cpp

client-phase2: client-phase2.cpp
	g++ -o client-phase2 -pthread client-phase2.cpp

client-phase3: client-phase3.cpp
	g++ -o client-phase3 -pthread client-phase3.cpp -lcrypto

client-phase4: client-phase4.cpp
	g++ -o client-phase4 -pthread client-phase4.cpp

client-phase5: client-phase5.cpp
	g++ -o client-phase5 -pthread client-phase5.cpp -lcrypto

