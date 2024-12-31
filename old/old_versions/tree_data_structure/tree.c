#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void) {

	int array[4096] = {0};	

	/// trying to make a tree using an array

}

/*

	so the basic way that we are going to try constructing this data structure is the following:


		[c, c, c, c, c]


		so we need to somehow have a set of natural numbers which are all asosicated together,    
			like, a struct.  so we need an array of structs, first, i think. this can trivially be made into an array of natural numbers


		

	so this will be out struct definition:


		struct element {
			int child;
			int count;
			int parent;
			int length;
		//	int insert;      // we don't actually need this one, because we just use the sign of length.
			int post_c;
			int post_o;
			int pre_c;
			int pre_o;
		};

	something like that 

				where post* and pre* are the origin and cursor states before and after the given operation.  



					note, that if		length     is less than zero, then we are deleting that number of characters, 

						and the characters that we deleted will be in   .text
				

					if length is positive,  

							then we will be inserting the text in .text 

				so yeah



						pretty simple encoding of  the bool insert member



			yay



						the hard part is the children of the node in the tree. 



						each node needs a number of children, 

							a parent, 


								and the    INDICIES    of its childen, in the file. 



										in this case, in the array. 



								and this shoulddd be able to be editable. thats the hard part. 






			because we might want to increase the child count of a given node.  thats the troublesome part. 












	.....wait. 




								what if we took a different approach to encoding this 







			what if we didnt even record the children, 



						just the parents!?!?!





			and then traversing the tree, was like a very expensive operation???





		hmmmm... idk 





						because like, we reallyyyyyyyyyy don't care how long it takes to traverse the tree, 



								like, that doesnt matter         AT ALL 







				so i think we can safely make that a linear operation,   instead of actually encoding the children like verbatim. 





				this simpilifies the problem so much, to the point that we actaully don't even need two files anymore!!!



				we just need one 




	and at that point, everything becomes easy, actually 






						LITERALLYYYYY ALL WE DO


					to encode the tree 




				is just denote the parent relationships. 



					wow. 



						that is so cool. 








			so yeah, undoing is fast, 




					but redoing is actually incredibly slowwwwwwww               and thats fine lol







			because it literally doesnt matter how fast we can redo lol


			and its probably still going to be super fast anyways lol. 




	so yeah. 




									yay







			okay this is the way we are going to do things, then. this simplifies the whole thing, and then now, this whole c program isnt even neccessary anymore, because its trivial to maket he whole .history file use a single file, actually. 









								yay






	this trick of encoding a tree as a          "parent-only" connection map 



							ie, a node doesnt know what its children are,   only who is its parent 



					in order to make the encoding of each node    only take up a constant amount of space, 




				now, 


					in reality, the node will still be required to variable in size, becusae of the .text member 



								that needs to be there 




		but thats a simple fix 

				as long as we use file index as our parent pointers, then we are fine. 






								ALSOOOOOO







			this solves ANOTHER PROBLEM  that we were running up against,   which was 



							the fact that we needed to make the node's children array variable 


						ie, dynamic,   because when we added a new branch in the undo tree, that would require editing an aleady generated node!!!!





								and like, thats kind of a no no.




							because like, we would have to reindex everything in the file. and thats computationally expensive, complicated, error prone, and slow 



									and not worth it lol



				so instead, 


					we just encode the tree in such a way that we make it fully   static 


						once a node is generated,      its data never         NEVER    literally            needs to changed again.




which 					is highly helpful and desirable. 





							also i just found a visual bug in the editor,       i think it occurs when the wrap width is exactly the sam as the line width. not sure. it was super weird. it was a desync bug. 




				OHHHHH did i mention i am using my own editor to type this!!!    its actaully working so well 




				just trying to find out if there is anything i should reallyyyyy change about this thing




						besides making the undo tree of course, which is a work in progress lol





		one thing i love about this encoding of the undo tree    using only parent pointers, 




					is that 

							it means that undo'ing is still fast.   which is what i use the undo tree for,   90% of the time lol.

								more like 99% actually 





								thats why its called an undo tree, after all     lol 



					to be able to undo 




				and because we are using parent pointers,    that will still be a constant time operation in order go to the previous history node. 

							






					oh, i should mention that      the "head" variable    is encoded at the top of the file.  so all node data and indicies start from there, after that 



								just a small detail, though, really. doesnt change the algorithm much,  i think 



		yay





		i am happy we figured this out.   




