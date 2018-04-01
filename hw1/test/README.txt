[How to use test]

	Platform: linux (CSIE workstation)

	./test.sh [read_server_port] [write_server_port]

-----------------------------------------------------

	1. change the permission of test.sh so that you can run it

		chmod +x test.sh
	
	2. you need to place write_server and read_server in the same directory

		[they are all in the same directory] 
				
			test test.sh write_server read_server

	3. test.sh will print some info about your servers and output the scores. if you got all 1s (6 1s), you get full score of this assignment.

	4. remember to zip you execute file(i.e., test) before you upload to
	workstation
