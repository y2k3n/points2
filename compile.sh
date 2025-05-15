# clang++ -O3 p2-steensgaard.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o p2-steensgaard

clang++ -O3 p2.cpp `llvm-config --cxxflags --ldflags --system-libs --libs core` -o p2

# clang++ -O3 live.cpp -DNO_OUTPUT -DLIVE_CONCURRENT -DPSTATS `llvm-config --cxxflags --ldflags --system-libs --libs core` -o live-c