/*
Copyright (c) 2015 Alex Morgan

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:
The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/


#include <string>
#include <regex>
#include <iostream>
#include <fstream>
#include <map>
#include <stdlib.h>

using namespace std;

class IncludeException {
};
class BadArgumentException : public IncludeException {
public:
  BadArgumentException(string arg) {
    this->arg = arg;
  }
  string arg;
};
class FileException : public IncludeException {
public:
  FileException(string path) {
    this->path = path;
  }
  string path;
};
class EnvironmentException : public IncludeException {
public:
  EnvironmentException(string name) {
    this->name = name;
  }
  string name;
};
class SyntaxException : public IncludeException {
public:
  SyntaxException(string line) {
    this->line = line;
  }
  string line;
};
class UndefinedMacroException : public IncludeException {
public:
  UndefinedMacroException(string name) {
    this->name = name;
  }
  string name;
};

class Includer {
public:
  Includer();
  void processArgument(const string &s);
  void processDefineArgument(const string &s);
  void processPredicate(const string &s);
  void processFile(const string &filePath);
  void processLine(string line, ifstream &file);
  string processBlock(ifstream &file, bool activated);
  bool isEndLine(const string &line);
  bool conditionOnLine(string line);
  void processConditional(ifstream &file, string line);
  void defineMacro(string name, string value);
  void lookForEnvironmentVariable(string name);
  bool isVarTruthy(string name);
private: 
  map<string, string> macros;
  bool ifBranchFound;
  bool inSuppressedBlock;
};

Includer::Includer() {
  ifBranchFound = false;
  inSuppressedBlock = false;
}

void Includer::processArgument(const string &arg) {
  if (arg.at(0) == '-') {
    switch (arg.at(1)) {
    case 'D': 
      processDefineArgument(arg);
      break;
    case 'P':
      processPredicate(arg);
      break;
    default:
      throw BadArgumentException(arg);
    }
  }
  else {  // anything that isn't a flag on the command line is a file name
    try {
      processFile(arg);
    }
    catch ( IncludeException e) {
      cerr << "Error processing file: " + arg << endl;
      throw;
    }
  }
}


// syntax of a define argument is -Dname="value"
void Includer::processDefineArgument(const string &s) {
  regex dArgPattern("-D(\\w+)=(.*)");
  smatch match;
  if (regex_search(s, match, dArgPattern)) {
    defineMacro(match.str(1), match.str(2));
  }
  else {
    throw BadArgumentException(s);
  }
}

void Includer::processPredicate(const string &s) {
  regex pArgPattern("-P(\\w+)");
  smatch match;
  if (regex_search(s, match, pArgPattern)) {
    defineMacro(match.str(1), "TRUE");
  }
  else {
    throw BadArgumentException(s);
  }
}

void Includer::processFile(const string &filePath) {
  ifstream file(filePath.c_str(), ios::in);
  if (!file.is_open()) {
    cerr << "Could not open " << filePath << '\n';
    throw FileException(filePath);
  }
  string line;
  while ( getline(file, line) ) {
    try {
      processLine(line, file);
    }
    catch (IncludeException e) {
      cerr << "Error on this line: " << line << endl;
      throw;
    }
  }
  file.close();
}

void Includer::defineMacro(string name, string value) {
  macros[name] = value;
}

void Includer::lookForEnvironmentVariable(string name) {
  char *value = getenv(name.c_str());
  if (value) {
    defineMacro(name, string(value));
  }
  else {
    cerr << "Environment variable not found: " << name << endl;
    throw EnvironmentException(name);
  }
}

// Empty string, '0', and 'false' are false
// Any other value is true
// I'm not handling case conversion here. So, note, "FALSE" will be true!!!
bool Includer::isVarTruthy(string value) {
  if (value.length() < 1) return false;
  if (value == "0") return false;
  if (value == "false") return false;
  return true;
}

regex hashPattern("##");
regex macroPattern("##(\\w+)##");
regex includeLinePattern("^##include (.+)");
regex defineLinePattern("^##define (\\w+) (.*)");
regex environmentLinePattern("^##env (\\w+)");

regex ifPattern("^##if(?: (.+))?");
regex elseifPattern("^##elseif(?: (.+))?");
regex elsePattern("^##else *$");
regex endPattern("^##end");

string Includer::processBlock(ifstream &file, bool activated) {
  while (1) {
    string line;
    if ( getline(file, line) ) {
      if (regex_search(line, elseifPattern) || regex_search(line, elsePattern) || regex_search(line, endPattern)) {
	return line;
      }
      else if (activated) {
	processLine(line, file);
      }
    }
    else {
      throw SyntaxException("No ##end terminating block");
    }
  }
}

bool Includer::isEndLine(const string &line) {
  return regex_search(line, endPattern);
}

bool Includer::conditionOnLine(string line) {
  smatch match;
  if (regex_search(line, match, elsePattern)) {
    return true;
  }
  while (regex_search(line, match, macroPattern)) {
    string key = match.str(1);
    try {
      string value = macros.at(key);
      line = match.prefix().str() + value + match.suffix().str();
    }
    catch (out_of_range& err) {
      // In the context of evaluating a conditional, an undefined macro just translates to an empty string and is not an error
      line = match.prefix().str() + match.suffix().str();
    }
  }
  if (regex_search(line, match, elseifPattern) || regex_search(line, match, ifPattern)) {
    return isVarTruthy(match.str(1));
  }
  else {
    throw SyntaxException(line);
  }
}

void Includer::processConditional(ifstream &file, string line) {
  bool foundCondition = false;
  do {
    bool runThisBlock;
    if (foundCondition) {
      runThisBlock = false; // out of a set of if/elseif/else blocks, run only first with satisfied condition
    }
    else {
      foundCondition = runThisBlock = conditionOnLine(line);
    }
    line = processBlock(file, runThisBlock);
  } while (!isEndLine(line));
}

void Includer::processLine(string line, ifstream &file) {
  smatch match;
  if (regex_search(line, match, ifPattern)) {  
    processConditional(file, line);
    return;
  }
  if (regex_search(line, match, hashPattern)) { // if any ## anywhere
    if (regex_search(line, match, includeLinePattern)) {
      processFile(match.str(1));
      return;
    }
    else if (regex_search(line, match, defineLinePattern)) {
      defineMacro(match.str(1), match.str(2));
      return;
    }
    else if (regex_search(line, match, environmentLinePattern)) {
      lookForEnvironmentVariable(match.str(1));
      return;
    }
    string prefix = "";
    while (regex_search(line, match, macroPattern)) {
      string key = match.str(1);
      if (key == "HASHHASH") {
	prefix = prefix + match.prefix().str() + "##";
	line = match.suffix().str();
      }
      else {
	try {
	  string value = macros.at(key);
	  line = match.prefix().str() + value + match.suffix().str();
	}
	catch (out_of_range& err) {
	  throw UndefinedMacroException(key);
	}
      }
    }  // END while macros in line
    line = prefix + line;
  }
  cout << line << endl;
}
 
int main(int argc, char *argv[]) {
  Includer include;
  int i;
  for (i = 1; i < argc; ++i) {
    string arg = string(argv[i]);
    try {
      include.processArgument(arg);
    }
    catch (IncludeException e) {
      cerr << "Error processing argument: " << arg << endl;
      return 1;
    }
  } 
  return 0;
}
