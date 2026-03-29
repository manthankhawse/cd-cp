#!/bin/bash
set -e

cd "$(dirname "$0")"

echo "=== Compiling Track 1: Lex/Flex ==="
cd lex_flex
flex policy_lexer.l
gcc -O3 -c lex.yy.c
gcc -O3 -c benchmark.c
gcc -O3 -o benchmark_flex lex.yy.o benchmark.o
cd ..

echo "=== Compiling Track 3: Native C++ ==="
cd cpp_combinator
g++ -O3 benchmark.cpp -o benchmark_cpp
cd ..

echo "=== Preparing Track 2: ANTLR4 (Java) ==="
cd antlr
if [ ! -f "antlr-4.13.1-complete.jar" ]; then
    echo "Downloading ANTLR4 runtime (~2.3MB)..."
    wget -q https://www.antlr.org/download/antlr-4.13.1-complete.jar
fi
java -jar antlr-4.13.1-complete.jar CloudPolLexer.g4
javac -cp ".:antlr-4.13.1-complete.jar" *.java
cd ..

echo "=== Generating 10,000 lines test policy ==="
cat << 'EOF' > test_policy_chunk.pol
ALLOW ROLE "Admin" ACTION "s3:*" ON RESOURCE "*";
DENY ROLE "Guest" ACTION "iam:*" ON RESOURCE "*" WHERE ip == "0.0.0.0" AND time > 100;
EOF

rm -f test_policy.pol
for i in {1..5000}; do
    cat test_policy_chunk.pol >> test_policy.pol
done
rm test_policy_chunk.pol

echo "Test file generated: test_policy.pol (~10,000 lines)"
echo ""
echo "=========================================="
echo "          BENCHMARK RESULTS"
echo "=========================================="
echo ""

ITERATIONS=50

echo "1. Running Flex (C)..."
./lex_flex/benchmark_flex test_policy.pol $ITERATIONS

echo ""
echo "2. Running Native Combinator (C++)..."
./cpp_combinator/benchmark_cpp test_policy.pol $ITERATIONS

echo ""
echo "3. Running ANTLR4 (Java)..."
cd antlr
java -cp ".:antlr-4.13.1-complete.jar" Benchmark ../test_policy.pol $ITERATIONS
cd ..
echo ""
echo "=========================================="
