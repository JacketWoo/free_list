#include "free_list.h"

#include <stdint.h>

#include <string>
#include <iostream>

int main(int argc, char* argv[]) {
  flist::Flist<uint32_t, std::string> ml;
  ml.head()->key = -1;

  //std::cout << ml.head()->succ.load().mark << std::endl; 

  ml.Insert(0, "0");
  ml.Insert(1, "1");
  ml.Insert(2, "2");
  ml.Insert(3, "3");
  ml.Insert(4, "4");
  ml.Insert(5, "5");
  ml.Insert(6, "6");
  ml.Insert(7, "7");
  ml.Insert(8, "8");
  ml.Insert(9, "9");

  flist::Node<uint32_t, std::string>* mn = NULL;
  for (int idx = 0; idx != 10; ++idx) {
    mn = ml.Search(idx);
    if (mn) {
      std::cout << "Key: " << mn->key << ", ele: " << mn->element << std::endl;
    } else {
      std::cout << "Key: " << mn->key << ", ele: " << mn->element << std::endl;
    }
  }
  return 0;
}
