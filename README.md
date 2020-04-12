# include-tool
a command-line text-processing tool

Does some basic processing on text files, similar in functionality to what the good ol' C preprocessor would do (but for 
HTML, etc, etc). 

Reads each line from each file listed on the command line. If a line begins with ## it is processed as a directive. 
Directives include:<br/>
##define<br/>
##env<br/>
##include<br/>
##if<br/>
##elseif<br/>
##else<br/>
##end<br/>

A ##define directive must be of the form:
<pre>
##define KEY=value
</pre>
and creates a macro.

An ##env directive must be of the form:
<pre>
##env varname
</pre>
When the ##env directive is encountered, the environment variables are searched for one with the given name, and a macro is 
created to substitute that environment variable's value when its name is encountered.

An ##include directive must be of the form:
<pre>
##include filename
</pre>
and causes the named file to be processed. 

An ##if directive must be of the form:
<pre>
##if ##MACRONAME##
</pre>
The following lines, up to the next ##elseif, ##else, or ##end, will either be processed or ignored, depending on the boolean
value of the named macro: if it isn't defined, or is defined as either 0 or false, it's considered false; if it's defined 
with any value, it's considered true. You can guess what ##elseif and ##else do. The final block must be terminated with an
##end directive. (Note: Unlike in any real programming language, an ##if directive cannot be embedded inside a block. 
Processing of these conditional directives is not recursive. Possibly that could be easily fixed?
The result of an ##if followed by another ##if without an intervening ##end is undefined.) 

Any line that isn't ignored, and isn't a directive, has any macro names substituted with their respective values, and then
is copied to standard out. Macro names are surrounded by double hashes and can appear anywhere on a line. In this context,
an undefined macro is an error. 

Command-line flags:

-Dname=value causes a macro to be created, defining _name_ as _value_.

-Pname causes a macro to be created, defining _name_ as TRUE.

Flags and file names can be intermingled on the command line; i.e. you don't have to list all flag first and then all files.
Flags apply only to the processing of files that appear after the flag on the command line. For example, given the command
line:
<pre>
include file1.txt -PFOO file2.txt
</pre>
First file1.txt is processed, with FOO undefined, and then file2.txt is processed, with FOO defined as TRUE.

