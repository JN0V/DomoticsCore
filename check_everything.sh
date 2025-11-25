#!/bin/bash

echo "Starting full validation check..."

chmod +x build_all_examples.sh
chmod +x run_all_tests.sh

echo "1. Building all examples..."
if ./build_all_examples.sh; then
    echo "✅ Examples build success"
else
    echo "❌ Examples build failed"
    exit 1
fi

echo "2. Running unit tests..."
if ./run_all_tests.sh; then
    echo "✅ Unit tests success"
else
    echo "❌ Unit tests failed"
    exit 1
fi

echo "✅ ALL CHECKS PASSED"
