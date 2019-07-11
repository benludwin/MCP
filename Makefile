mcp:
	gcc -std=c99 -g -o part1 part1.c
	gcc -std=c99 -g -o part2 part2.c
	gcc -std=c99 -g -o part3 part3.c
	gcc -std=c99 -g -o part4 part4.c
	gcc -std=c99 -g -o part5 part5.c
clean:
	rm part1
	rm part2
	rm part3
	rm part4
	rm part5
