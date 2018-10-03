CXXFLAGS+= -std=c++17

all: constraining-concepts-overload-sets.html

%.html: %.md
	( echo "<html><body bgcolor=white text=black>" ; cmark $< ) > $@
