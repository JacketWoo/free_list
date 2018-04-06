#include "free_list.h"

#include <stdint.h>

#include <string>
#include <iostream>

struct StEd {
  flist::Flist<uint32_t, std::string>* fl; 
  uint32_t st;
  uint32_t ed;
};

static void* ThreadMain(void* arg) {
  StEd* se = reinterpret_cast<StEd*>(arg);
  for (uint32_t idx = se->st; idx != se->ed; ++idx) {
    se->fl->Insert(idx, std::to_string(idx));
  }
  delete se;
}

#define THREAD_NUM 10 
#define EACH_NUM   500
int main(int argc, char* argv[]) {
  flist::Flist<uint32_t, std::string> fl;
  fl.head()->key = -1;

  pthread_t tid[THREAD_NUM];
  for (uint32_t idx = 0; idx != THREAD_NUM; ++idx) {
    StEd* se = new StEd{&fl, EACH_NUM*idx, EACH_NUM*(idx+1)};
    pthread_create(&tid[idx], NULL, ThreadMain, se);
  }
  for (uint32_t idx = 0; idx != THREAD_NUM; ++idx) {
    pthread_join(tid[idx], NULL);
  }

  //flist::Node<uint32_t, std::string>* fn = nullptr;
  //for (uint32_t idx = 0; idx != EACH_NUM*THREAD_NUM; ++idx) {
  //  fn = fl.Search(idx);
  //  if (fn) {
  //    std::cout << "Key: " << fn->key << ", ele: " << fn->element << std::endl;
  //  } else {
  //    std::cout << "Key: " << fn->key << ", ele: " << fn->element << std::endl;
  //  }
  //}

  return 0;
}
