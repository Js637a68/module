obj = test.o threadpool.o

test : $(obj)
	$(CXX) -g $^ -o $@ -lpthread

%.o : %.cpp
	$(CXX) -c -g $< -o $@

clean:
	rm -rf *.o test 
