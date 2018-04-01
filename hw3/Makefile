all: file_reader file_reader_slow server
file_reader: file_reader.c 
	cc -Wall file_reader.c -o file_reader
file_reader_slow: file_reader.c 
	cc -Wall file_reader.c -DSLOW -o file_reader_slow
server: server.c
	cc -Wall server.c -o server
clean:
	rm file_reader file_reader_slow server
