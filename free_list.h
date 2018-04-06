#ifndef FREE_LIST_H
#define FREE_LIST_H

#include <stdint.h>
#include <string.h>

#include <atomic>

namespace flist {

template<typename T>
T cas(std::atomic<T>* address, T old_val, T new_val) {
  if (address->compare_exchange_strong(old_val, new_val)) {
    return old_val;
  }
  //TODO: here may not atomic
  return *address;
}


template<typename Key, typename Element>
struct Node;

template<typename Key, typename Element>
struct Successor {
  bool operator==(const Successor& succ) {
    return !memcmp(this, &succ, sizeof(*this));
  }
  Node<Key, Element>* right;
  uint64_t mark;
  uint64_t flag;
};

template<typename Key, typename Element>
struct Node {
public:
  Node(const Key& k,
       const Element& e) :
      key(k),
      element(e),
      backlink(nullptr),
      succ(Successor<Key, Element>{nullptr, 0, 0}) {
  }
  Node() :
      backlink(nullptr),
      succ(Successor<Key, Element>{nullptr, 0, 0}) {
  }


  Key key;
  Element element;
  Node<Key, Element>* backlink;
  std::atomic<Successor<Key, Element> > succ;
private:
  Node& operator=(const Node& node);
};

template<typename Key, typename Element>
struct NodePos {
  Node<Key, Element>* pre_node;
  Node<Key, Element>* next_node;
};

template<typename Key, typename Element>
struct FlagRes {
  Node<Key, Element>* pre_node;
  bool result;
};

template<typename Key, typename Element> 
class Flist {
public:
  Flist();
  ~Flist();

  Node<Key, Element>* Search(const Key& k);
  Node<Key, Element>* Delete(const Key& k);
  Node<Key, Element>* Insert(const Key& k,
                             const Element& e);
  Node<Key, Element>* head() {
    return head_;
  };
private:
  //Finds two consective nodes n1 and n2
  //such that n1.key <= k < n2.key 
  NodePos<Key, Element> SearchFrom(const Key& k,
                                   Node<Key, Element>* curr_node);
  NodePos<Key, Element> SearchFromD(const Key& k,
                                    Node<Key, Element>* curr_node);
  void TryMark(Node<Key, Element>* del_node);
  void HelpMarked(Node<Key, Element>* pre_node,
                  Node<Key, Element>* del_node);

  FlagRes<Key, Element> TryFlag(Node<Key, Element>* pre_node,
                  Node<Key, Element>* target_node);
  void HelpFlagged(Node<Key, Element>* pre_node,
                   Node<Key, Element>* del_node);

  Node<Key, Element>* head_;
};

template<typename Key, typename Element>
Flist<Key, Element>::Flist() {
  head_ = new Node<Key, Element>();
}

template<typename Key, typename Element>
Flist<Key,  Element>::~Flist() {
  Node<Key, Element>* node = nullptr;
  while (head_->succ.load().right) {
    node = Delete(head_->succ.load().right->key);
    delete node;
  }

  //TODO: this may occur contention error
  delete head_; 
}

template<typename Key, typename Element>
Node<Key, Element>* Flist<Key, Element>::Search(const Key& k) {
  NodePos<Key, Element> pos = SearchFrom(k, head_);
  if (pos.pre_node->key == k) {
    return pos.pre_node;
  }
  return nullptr;
}

template<typename Key, typename Element>
Node<Key, Element>* Flist<Key, Element>::Delete(const Key& k) {
  NodePos<Key, Element> pos = SearchFromD(k, head_);
  if (pos.next_node->key != k) {
    return nullptr;
  }
  FlagRes<Key, Element> fr = TryFlag(pos.pre_node, pos.next_node);
  if (fr.pre_node) {
    HelpFlagged(fr.pre_node, pos.next_node);
  }
  if (!fr.result) {
    return nullptr;
  }
  return pos.next_node;
}

template<typename Key, typename Element>
Node<Key, Element>* Flist<Key, Element>::Insert(const Key& k,
                                                const Element& e) {
  NodePos<Key, Element> pos = SearchFrom(k, head_);
  if (pos.pre_node->key == k) {
    return pos.pre_node; 
  }

  Node<Key, Element>* new_node = new Node<Key, Element>(k, e);

  while (true) {
    std::atomic<Successor<Key, Element> >& pre_succ = pos.pre_node->succ;
    if (pre_succ.load().flag == 1) {
      HelpFlagged(pos.pre_node, pre_succ.load().right);
    } else {
      new_node->succ.store(Successor<Key, Element>{pos.next_node, 0, 0});
      Successor<Key, Element> result = cas<Successor<Key, Element> >(&pos.pre_node->succ,
                                                                     Successor<Key, Element>{pos.next_node, 0, 0},
                                                                     Successor<Key, Element>{new_node, 0, 0});
      if (result == Successor<Key, Element>{pos.next_node, 0, 0}) {
        return new_node;
      } else {
        if (!result.mark
            && result.flag == 1) {
          HelpFlagged(pos.pre_node, result.right);
        }
        while (pos.pre_node->succ.load().mark == 1) {
          pos.pre_node = pos.pre_node->backlink; 
        }
      }
    }
    pos = SearchFrom(k, pos.pre_node);
    if (pos.pre_node->key == k) {
      delete new_node;
      return pos.pre_node;
    }
  }

  // Never reach here
  return nullptr;
}

template<typename Key, typename Element>
NodePos<Key, Element> Flist<Key, Element>::SearchFrom(const Key& k,
                                                      Node<Key, Element>* curr_node) {
  Node<Key, Element>* next_node = curr_node->succ.load().right;
  while (next_node
         && next_node->key <= k) {
    // Ensure that either next_node is unmarked,
    // or both curr_node and next_node are
    // marked and curr_node was marked ealier
    while (next_node
           && next_node->succ.load().mark == 1
           && (!curr_node->succ.load().mark
                || curr_node->succ.load().right != next_node)) {
      if (curr_node->succ.load().right == next_node) {
        HelpMarked(curr_node, next_node);
      }
      next_node = curr_node->succ.load().right;
    }
    if (!next_node) {
      break;
    }
    if (next_node->key <= k) {
      curr_node = next_node;
      next_node = curr_node->succ.load().right;
    }
  }
  return {curr_node, next_node};
}

template<typename Key, typename Element>
NodePos<Key, Element> Flist<Key, Element>::SearchFromD(const Key& k,
                                                       Node<Key, Element>* curr_node) {
  Node<Key, Element>* next_node = curr_node->succ.load().right;
  while (next_node
         && next_node->key < k) {
    // Ensure that either next_node is unmarked,
    // or both curr_node and next_node are
    // marked and curr_node was marked ealier
    while (next_node
           && next_node->succ.load().mark == 1
           && (!curr_node->succ.load().mark
                or curr_node->succ.load().right != next_node)) {
      if (curr_node->succ.load().right == next_node) {
        HelpMarked(curr_node, next_node);
      }
      next_node = curr_node->succ.load().right;
    }
    if (!next_node) {
      break;
    }
    if (next_node->key < k) {
      curr_node = next_node;
      next_node = curr_node->succ.load().right;
    }
  }
  return {curr_node, next_node};
}

template<typename Key, typename Element>
void Flist<Key, Element>::TryMark(Node<Key, Element>* del_node) {
  Node<Key, Element>* next_node = nullptr;
  Successor<Key, Element> succ;
  do {
    next_node = del_node->succ.load().right;
    succ = cas<Successor<Key, Element> >(&del_node->succ, {next_node, 0, 0}, {next_node, 1, 0});
    if (!succ.mark && (succ.flag == 1)) {
      HelpFlagged(del_node, del_node->succ.load().right); 
    }
  } while (!del_node->succ.load().mark);
}

template<typename Key, typename Element>
void Flist<Key, Element>::HelpMarked(Node<Key, Element>* pre_node,
                                     Node<Key, Element>* del_node) {
  // Attemps to physically delete the marked
  // node del_node and unflag pre_node
  Node<Key, Element>* next_node = del_node->succ.load().right; 
  cas<Successor<Key, Element> >(&pre_node->succ, {del_node, 0, 1}, {next_node, 0, 0});
}

template<typename Key, typename Element >
FlagRes<Key, Element> Flist<Key, Element>::TryFlag(Node<Key, Element>* pre_node,
                                                   Node<Key, Element>* target_node) {
  // Attemps to flag the predecessor of target_node.
  // pre_node is the last node known to be the predecessor
  Successor<Key, Element> result;
  while (true) {
    if (pre_node->succ.load() == Successor<Key, Element>{target_node, 0, 1}) {
      return {pre_node, false};
    }
    result = cas<Successor<Key, Element> >(&pre_node->succ, {target_node, 0, 0}, {target_node, 0, 1});
    if (result == Successor<Key, Element>{target_node, 0, 0}) {
      return {pre_node, true};
    }
    if (result == Successor<Key, Element>{target_node, 0, 1}) {
      return  {pre_node, false};
    }
    while (pre_node->succ.load().mark == 1) {
      pre_node = pre_node->backlink;
    }
    NodePos<Key, Element> pos = SearchFromD(target_node->key, pre_node);
    if (pos.next_node != target_node) {
      return {nullptr, false};
    }
  }
  // Never reach here
  return {nullptr, false};
}

template<typename Key, typename Element>
void Flist<Key, Element>::HelpFlagged(Node<Key, Element>* pre_node,
                                      Node<Key, Element>* del_node) {
  // Atempts to mark and physically delete node del_node
  // which is the successor of the flagged node pre_node

  //TODO: here evaluation happed, whether need lock
  del_node->backlink = pre_node;
  if (!del_node->succ.load().mark) {
    TryMark(del_node);
  }
  HelpMarked(pre_node, del_node);
}

}
#endif
