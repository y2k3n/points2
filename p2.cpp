#include "llvm/IR/Argument.h"
#include "llvm/IR/Function.h"
#include "llvm/IR/Instruction.h"
#include "llvm/IR/Instructions.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/User.h"
#include "llvm/IR/Value.h"
#include "llvm/IRReader/IRReader.h"
#include "llvm/Support/InitLLVM.h"
#include "llvm/Support/SourceMgr.h"
#include "llvm/Support/raw_ostream.h"

#include <queue>
#include <set>
#include <unordered_map>
#include <chrono>

using namespace llvm;

std::unordered_map<Value *, std::set<Value *>> pt;
std::queue<std::pair<Value *, std::set<Value *>>> worklist;
std::unordered_map<Value *, std::set<Value *>> PFG;

void addEdge(Value *s, Value *t) {
  if (PFG[s].find(t) == PFG[s].end()) {
    PFG[s].insert(t);
    if (!pt[s].empty()) {
      worklist.push({t, pt[s]});
    }
  }
}

void propagate(Value *n, const std::set<Value *> &pts) {
  if (!pts.empty()) {
    pt[n].insert(pts.begin(), pts.end());
    for (auto *s : PFG[n]) {
      worklist.push({s, pts});
    }
  }
}

void initialize(Function &func) {
  for (auto &BB : func) {
    for (auto &inst : BB) {

      if (auto *alloca = dyn_cast<AllocaInst>(&inst)) {
        worklist.push({alloca, {alloca}});

      } else if (auto *gep = dyn_cast<GetElementPtrInst>(&inst)) {
        worklist.push({gep, {gep}});

      } else if (auto *phi = dyn_cast<PHINode>(&inst)) {
        for (int i = 0; i < phi->getNumIncomingValues(); ++i) {
          Value *val = phi->getIncomingValue(i);
          if (isa<Instruction>(val) || isa<Argument>(val)) {
            addEdge(val, phi);
          }
        }

      } else if (auto *select = dyn_cast<SelectInst>(&inst)) {
        Value *tval = select->getTrueValue();
        Value *fval = select->getFalseValue();
        if (isa<Instruction>(tval) || isa<Argument>(tval)) {
          addEdge(tval, select);
        }
        if (isa<Instruction>(fval) || isa<Argument>(fval)) {
          addEdge(fval, select);
        }

      } else if (auto *cast = dyn_cast<CastInst>(&inst)) {
        Value *src = cast->getOperand(0);
        addEdge(src, cast);
      }

      // iter end
    }
  }
}

void solve() {
  while (!worklist.empty()) {
    auto [n, pts] = worklist.front();
    worklist.pop();

    std::set<Value *> delta;
    std::set_difference(pts.begin(), pts.end(), pt[n].begin(), pt[n].end(),
                        std::inserter(delta, delta.begin()));
    propagate(n, delta);

    for (auto *user : n->users()) {
      if (StoreInst *store = dyn_cast<StoreInst>(user)) {
        // *x = y (store y -> ptr x)
        if (store->getPointerOperand() == n) {
          Value *y = store->getValueOperand();
          if (isa<Instruction>(y) || isa<Argument>(y)) {
            for (Value *oi : delta) {
              addEdge(y, oi);
            }
          }
        }

      } else if (LoadInst *load = dyn_cast<LoadInst>(user)) {
        // y = *x (load ptr x -> y)
        if (load->getPointerOperand() == n) {
          Value *y = load;
          for (Value *oi : delta) {
            addEdge(oi, y);
          }
        }
      }
    }
    // iter end
  }
}

void print() {
  outs() << "Points-to Set:\n";
  outs() << "=================\n";
  for (auto &[p, points2] : pt) {
    outs() << *p << "\n->";
    for (Value *v : points2) {
      outs() << "\t" << *v << "\n";
    }
    outs() << "\n";
  }

  // outs() << "Pointer Flow Graph:\n";
  // outs() << "=================\n";
  // for (auto &[from, toSet] : PFG) {
  //   outs() << *from << "\n->";
  //   for (Value *to : toSet) {
  //     outs() << "\t" << *to << "\n";
  //   }
  //   outs() << "\n";
  // }
}

int main(int argc, char *argv[]) {
  InitLLVM X(argc, argv);
  if (argc < 2) {
    outs() << "Expect IR filename\n";
    exit(1);
  }
  LLVMContext context;
  SMDiagnostic smd;
  char *filename = argv[1];
  std::unique_ptr<Module> module = parseIRFile(filename, smd, context);
  if (!module) {
    outs() << "Cannot parse IR file\n";
    smd.print(filename, outs());
    exit(1);
  }

  outs() << "Intra-Procedural Analysis" << "\n";
  outs() << module->getFunctionList().size() << " function(s)\n";
  auto start = std::chrono::high_resolution_clock::now();

  for (auto &func : *module) {
    if (func.isDeclaration())
      continue;

    pt.clear();
    PFG.clear();

    initialize(func);
    solve();

#ifdef PRINT_RESULTS
    outs() << "\nFunction: " << func.getName() << "\n";
    print();
    outs() << "******************************** " << func.getName() << "\n";
#endif
  }

  auto end = std::chrono::high_resolution_clock::now();
  auto duration =
      std::chrono::duration_cast<std::chrono::microseconds>(end - start);
  outs() << "Analysis time: " << duration.count() << " us\n";
}