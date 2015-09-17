all: nim nim_server nim_match_server

nim: nim.c nim_msg.c
	gcc -g -Wall -o nim nim_msg.c nim.c

nim_server: nim_server.c nim_msg.c
	gcc -g -Wall -o nim_server nim_msg.c nim_server.c

nim_match_server: nim_match_server.c nim_msg.c
	gcc -g -Wall -o nim_match_server nim_msg.c nim_match_server.c
