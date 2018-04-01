1. Compile the server
        cc server.c -o server
        cc file_reader.c -o file_reader
        cc file_reader.c -DSLOW -o file_reader_slow
2. Run the server 
        ./server port tmp_file_name
3. Valid request
        http://your_ip:port/file_reader?file_name=name
        http://your_ip:port/info

