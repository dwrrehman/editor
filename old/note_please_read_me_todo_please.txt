todo:


. feature:   allow the user to give a custom name to the terminal tabs:   using     printf '\e]1;%s\a' 'My Tab'

      use whats on the clipboard, make this a command system   command.   "title"   or  "tabname".




. bug:  we are inserting   \r  into the file, when we never should be doing that.   display those as like... a control char or something.


. we NEEDDDD to make it so that when you save, it checks the    current last modified date   of the file, 
	
		and if its different from the last time you saved, it aborts the save, and chooses to save to 
		a new file instead, and as opposed to prompting you for a name, it just uses a random name, 
		and informs you that you just did that. so yeah. very improtant. please implement this, for safety. 


. make tab and enter insert their respective characters. its just efficient.   at LEAST do tab, please lol. plzzzz  dont do space though. 

. make  some way to quit the editor that ISNT QQ,  ie, use       s e q    or something like that   some lower case sequence. shouldnt be that hard.. lol. 

. make it so that when you move via text insertion  of characters, it modifies  the  desired column  var  in the right way. currently, it is not touched while inserting chars, but it should be. fix this. 





. make undo redo   center   the thing it changed. plz.





. make   ./build    output a line number for the first error    and jump to this automatically upon running the b key, 


		and/or,     make the b key actually kinda like      s  

				so that there are mulitple   subcommands


					ie, get rid of   v      everything is under b. 


				so, for isntance,    bb  would do ./build for copyb

						and br  would do ./run  for copya

				etc 
					many buffers, 
					many commands,  basicallyyyy regsiters.  super useful. 


					this way, we can have mulitple commands that are bound to keys, acessible by    b   X   where x is a key in the home row or alpha char of course. 


	so yeah. 




				make       s q       or     s w q     quit the editor, maybe. 
	
				oh, also allow for deleting something without pressing delete.


						ie,  like      s p d   for deleting text, 

							maybe even deleting the current selection, 
						and deleting a single char if its empty 



						so yeah!




	very useful. 




