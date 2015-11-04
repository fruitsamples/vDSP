#!/bin/csh -e

echo ""
echo "Building examples."
xcodebuild -configuration Default \
    -project vDSPExamples.xcodeproj \
    -target "All Examples"

echo ""
echo "Running Demonstrate."
./build/Default/Demonstrate

echo ""
echo "Running DTMF."
./build/Default/DTMF "159#"
