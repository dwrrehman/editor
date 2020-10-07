FLAGS = -O1 -Wall -fsanitize=address,undefined

editor: source/editor/main.c 
	gcc -g $(FLAGS) -I /usr/local/Cellar/llvm/10.0.1_1/include -L /usr/local/Cellar/llvm/10.0.1_1/lib -o editor source/editor/main.c -lclang 
