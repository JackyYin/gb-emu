#!/bin/bash

echo "Starting local server..."
echo "Open http://localhost:8080 in your browser"

cd "$(dirname "$0")/.."

if command -v python3 &> /dev/null; then
    python3 -m http.server 8080
elif command -v python &> /dev/null; then
    python -m http.server 8080
elif command -v npx &> /dev/null; then
    npx serve -p 8080
else
    echo "Error: No HTTP server found. Install Python or Node.js."
    exit 1
fi
