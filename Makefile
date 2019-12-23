TARGET = web_proxy

all : $(TARGET)

web_proxy:
	g++ -o $@ $@.cpp -lpthread

clean:
	rm -f web_proxy
