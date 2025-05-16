clang++ -O3 p2-steensgaard.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o p2-steensgaard

clang++ -O3 -g p2-inter-dense.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o p2-inter-dense

# clang++ -O3 -g p2-inter.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o p2-inter

# clang++ -O3 p2.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o p2
