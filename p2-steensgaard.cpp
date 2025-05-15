#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Value.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include <unordered_map>
#include <vector>

using namespace llvm;

class DisjointSet {
private:
  std::unordered_map<Value *, Value *> parent;
  std::unordered_map<Value *, int> rank;

public:
  std::unordered_map<Value *, Value *> points2;

  Value *find(Value *x) {
    if (parent.find(x) == parent.end()) {
      parent[x] = x;
      rank[x] = 0;
      return x;
    }
    if (x != parent[x]) {
      parent[x] = find(parent[x]);
    }
    return parent[x];
  }

  void union_(Value *p, Value *q) {
    Value *x = find(p);
    Value *y = find(q);
    if (x == y)
      return;
    if (rank[x] < rank[y]) {
      parent[x] = y;
    } else if (rank[x] > rank[y]) {
      parent[y] = x;
    } else {
      parent[y] = x;
      rank[x]++;
    }
  }

  std::unordered_map<Value *, std::vector<Value *>> groups() {
    std::unordered_map<Value *, std::vector<Value *>> groups;
    for (auto [key, val] : parent) {
      Value *root = find(val);
      groups[root].push_back(key);
    }
    return groups;
  }
};

void steensgaard(Instruction *inst, DisjointSet &ds) {
  if (auto *ac = dyn_cast<AllocaInst>(inst)) {
    ds.find(ac);
    ds.points2[ac] = ac;

  } else if (auto *ld = dyn_cast<LoadInst>(inst)) {
    // [p := *q] -> join(*p, **q)
    auto *q = ld->getPointerOperand();
    if (ds.points2.find(q) == ds.points2.end()) {
      ds.points2[q] = ld; // ?????
    } else {
      ds.union_(ld, ds.points2[q]);
    }

  } else if (auto *st = dyn_cast<StoreInst>(inst)) {
    // [*p := q] -> join(**p, *q)
    auto *p = st->getPointerOperand();
    auto *q = st->getValueOperand();
    if (ds.points2.find(p) == ds.points2.end()) {
      ds.points2[p] = q;
    } else {
      ds.union_(ds.points2[p], q);
    }
  } 
  // else if (auto *call = dyn_cast<CallInst>(inst)) {
  //   // join passed ptrs with ret val
  //   std::vector<Value *> vals;
  //   if (!call->getType()->isVoidTy()) {
  //     vals.push_back(call);
  //   }
  //   auto *cf = call->getCalledFunction();
  //   for (auto &arg : cf->args()) {
  //     if (arg.getType()->isPointerTy()) {
  //       vals.push_back(&arg);
  //     }
  //   }
  //   if (vals.size() <= 1)
  //     return;
  //   for (int i = 1; i < vals.size(); ++i) {
  //     ds.union_(vals[0], vals[i]);
  //   }

  // } else if (PHINode *phi = dyn_cast<PHINode>(inst)) {
  //   // join incoming ptrs with phi var
  //   for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
  //     auto *val = phi->getIncomingValue(i);
  //     if (val->getType()->isPointerTy()) {
  //       ds.union_(phi, val);
  //     }
  //   }
  // }
}

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);
  if (argc < 2) {
    errs() << "Expect IR filename\n";
    exit(1);
  }
  LLVMContext context;
  SMDiagnostic smd;
  char *filename = argv[1];
  std::unique_ptr<Module> module = parseIRFile(filename, smd, context);
  if (!module) {
    errs() << "Cannot parse IR file\n";
    smd.print(filename, errs());
    exit(1);
  }

  DisjointSet ds;
  for (auto &func : *module) {
    for (auto &BB : func) {
      for (auto &inst : BB) {
        steensgaard(&inst, ds);
      }
    }
  }

  auto groups = ds.groups();
  for (auto [key, group] : groups) {
    outs() << "{\n";
    for (auto val : group) {
      outs() << "\t" << *val << "\n";
    }
    outs() << "}\n";
  }
}