all: server client

server:
	g++ server.cpp -o sv

client:
	g++ client.cpp -o cl

clean:
	rm -f sv cl
