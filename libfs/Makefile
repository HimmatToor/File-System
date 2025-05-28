
lib := libfs.a

objs = fs.o disk.o

all: $(lib)

fs.o: fs.c disk.h fs.h
	gcc -Wall -Wextra -Werror -c fs.c -o fs.o

disk.o: disk.c disk.h
	gcc -Wall -Wextra -Werror -c disk.c -o disk.o

$(lib): $(objs)
	ar rcs $(lib) $(objs)

clean:
	rm -f $(lib) $(objs)
