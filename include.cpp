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

class Includer {
public:
  Includer();
  int processArgument(string s);
  void processDefineArgument(string s);
  void processPredicate(string s);
  int processFile(string filePath);
  int processLine(string line);
  void defineMacro(string name, string value);
  int lookForEnvironmentVariable(string name);
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

int Includer::processArgument(string arg) {
  if (arg.at(0) == '-') {
    switch (arg.at(1)) {
    case 'D': 
      processDefineArgument(arg);
      break;
    case 'P':
      processPredicate(arg);
      break;
    default:
      cerr << "Bad argument: " << arg << '\n';
      return 1;
    }
  }
  else {  // anything that isn't a flag on the command line is a file name
    if (processFile(arg)) {
      cerr << "Error processing file: " + arg << endl;
      return 1;
    }
  }
  return 0;
}


// syntax of a define argument is -Dname="value"
void Includer::processDefineArgument(string s) {
  regex dArgPattern("-D(\\w+)=(.*)");
  smatch match;
  if (regex_search(s, match, dArgPattern)) {
    defineMacro(match.str(1), match.str(2));
  }
  else {
    cerr << "Bad argument: " << s << endl;
  }
}


void Includer::processPredicate(string s) {
  regex pArgPattern("-P(\\w+)");
  smatch match;
  if (regex_search(s, match, pArgPattern)) {
    defineMacro(match.str(1), "TRUE");
  }
  else {
    cerr << "Bad argument: " << s << endl;
  }
}

int Includer::processFile(string filePath) {
  ifstream file(filePath.c_str(), ios::in);
  if (!file.is_open()) {
    cerr << "Could not open " << filePath << '\n';
    return 1;
  }
  string line;
  while ( getline (file,line) ) {
    if (processLine(line)) {
      cerr << "Error on this line: " << line << endl;
      return 1;
    }
  }
  file.close();
  return 0;
}

void Includer::defineMacro(string name, string value) {
  macros.insert(pair<string, string>(name, value));
}

int Includer::lookForEnvironmentVariable(string name) {
  char *value = getenv(name.c_str());
  if (value) {
    defineMacro(name, string(value));
    return 0;
  }
  else {
    cerr << "Environment variable not found: " << name << endl;
    return 1;
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

enum controlKeywords{ NONE, IF, ELSE, ELSEIF, END };
regex hashPattern("##");
regex controlPattern("^##((?:else)?(?:if)?(?:end)?)(.*)");
regex includeLinePattern("^##include (.+)");
regex defineLinePattern("^##define (\\w+) (.*)");
regex environmentLinePattern("^##env (\\w+)");
regex macroPattern("##(\\w+)##");

int Includer::processLine(string line) {
  bool hasMissingMacro = false;
  smatch match;
  if (inSuppressedBlock) {  // We ignore lines that are within a rejected if-else branch...
    if (regex_search(line, match, controlPattern)) {  // UNLESS this line actually ends that block
      inSuppressedBlock = false;
    }
    else {  // OK, so, didn't find i.e. ##else or ##end. Ignoring this line.
      return 0;
    }
  }
  if (regex_search(line, match, hashPattern)) { // if any ## anywhere
    if (regex_search(line, match, includeLinePattern)) {
      return processFile(match.str(1));
    }
    else if (regex_search(line, match, defineLinePattern)) {
      defineMacro(match.str(1), match.str(2));
      return 0;
    }
    else if (regex_search(line, match, environmentLinePattern)) {
      return lookForEnvironmentVariable(match.str(1));
    }
    while (regex_search(line, match, macroPattern)) {
      string key = match.str(1);
      try {
        string value = macros.at(key);
        line = match.prefix().str() + value + match.suffix().str();
      }
      catch (out_of_range& err) {
        hasMissingMacro = true;
        line = match.prefix().str() + match.suffix().str();
      }
    }
    // Now that we've substituted macro values, we can evaluate if's, elseif's. 
    // Handle all the conditional-block logic.
    if (regex_search(line, match, controlPattern)) {
      controlKeywords control = NONE;
      bool conditionalValue = true;
      if (match.str(1) == "if") {
        control = IF;
      }
      else if (match.str(1) == "else") {
        control = ELSE;
      }
      else if (match.str(1) == "elseif") {
        control = ELSEIF;
      }
      else if (match.str(1) == "end") {
        control = END;
      }
      if ((control == IF) || (control == ELSEIF)) {
        regex ifPattern("##(?:else)?if (.*)");
        if (regex_search(line, match, ifPattern)) {
          conditionalValue = isVarTruthy(match.str(1));
        }
        else {
          return 1;
        }
      }
      if ((!conditionalValue)) {
        inSuppressedBlock = true;
      }
      if ((control == ELSEIF || control == ELSE)  && ifBranchFound) {
        inSuppressedBlock = true;
      }
      if (conditionalValue) {
        if (control == END) {
          ifBranchFound = false;
        }
        else {
          ifBranchFound = true;
        }
      }
      return 0;
    }
  }
  if (hasMissingMacro) {
    return 1;
  }
  cout << line << endl;
  return 0;
}
 
int main(int argc, char *argv[]) {
  Includer include;
  int i;
  for (i = 1; i < argc; ++i) {
    string arg = string(argv[i]);
    if (include.processArgument(arg)) {
      cerr << "Error processing argument: " << arg << endl;
      return 1;
    }
  } 
  return 0;
}
