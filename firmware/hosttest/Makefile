SRC = test_host.cpp ../casio_deck/calc.cpp ../casio_deck/screen.cpp ../casio_deck/multitap.cpp
CXXFLAGS = -std=gnu++17 -Wall -Wextra -g -fsanitize=address,undefined

test: test_host
	./test_host

test_host: $(SRC) ../casio_deck/*.h
	$(CXX) $(CXXFLAGS) -o $@ $(SRC) -lm

clean:
	rm -f test_host

.PHONY: test clean
