# text_edit
Text Editor built using C

## Build

Clone the repository into your local machine and simply run ```make``` to generate the executable!

You can call ```./textedit ___.c``` to run any c file of your choice. This will provide you with a syntax highlighted view.

## Syntax Highlighting

To get Syntax Highlighting to work you can use the ```tree-sitter``` library and ```tree-sitter-c``` grammar. Simply clone their source repos into the root of this project and the makefile will take care of building the files you need. The links can be found here [tree-sitter](https://github.com/tree-sitter/tree-sitter) and [tree-sitter-c](https://github.com/tree-sitter/tree-sitter-c). 


