# include-tool
a command-line text-processing tool

Does some basic processing on text files, similar in functionality to what the good ol' C preprocessor would do (but for 
HTML, etc, etc). 

Reads each line from each file listed on the command line. If the line begins with ## it is processed as a directive. 
Directives include:
##define
##env
##include
##if
##elseif
##else
##end

A ##define directive must be of the form:
##define KEY=value
and creates a macro.

An ##env directive must be of the form:
##env varname
When the ##env directive is encountered, the environment variables are searched for one with the given name, and a macro is 
created to substitute that environment variable's value when its name is encountered.

An ##include directive 
