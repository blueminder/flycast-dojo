_retrieved from https://github.com/Renleilei1992/md5_

md5
===

Class to create MD5 checksum from file or string

<b>Example</b>

```c++

#include "md5/md5.h"

int main(int argc,char** argv){

  char cstring[] = "Foo baz, testing.";
  std::string str = cstring;

  /* MD5 from std::string */
  printf("md5sum: %s\n",  md5(  str ).c_str());
  
  /* MD5 from c-string */
  printf("md5sum: %s\n",  md5(  cstring ).c_str());
  
  /* Short MD5 from c-string */
  printf("md5sum6: %s\n", md5sum6( cstring ).c_str());
  
  /* Short MD5 from std::string */
  printf("md5sum6: %s\n", md5sum6( str ).c_str());
  
  /* MD5 from filename */
  printf("md5file: %s\n", md5file("README.md").c_str());
  
  /* MD5 from opened file */
  std::FILE* file = std::fopen("README.md", "rb");
  printf("md5file: %s\n", md5file(file).c_str());
  std::fclose(file);

  /* we're done */
  return EXIT_SUCCESS;
}

```

<b>Compilation in g++</b>

<i>g++ -std=c++0x -o md5 md5.cpp main.cpp</i>
