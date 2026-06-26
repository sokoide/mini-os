#!/bin/bash

echo "Testing keyboard input fixes in day11_completed..."
echo "Expected: All characters 'abc' should be received and processed"
echo ""

# Run QEMU with serial output and simulate some startup
timeout 5s qemu-system-i386 -drive file=os.img,format=raw,if=floppy -boot a -serial stdio -nographic &
QEMU_PID=$!

# Give QEMU time to start
sleep 2

# Kill QEMU
kill $QEMU_PID 2>/dev/null || true
wait $QEMU_PID 2>/dev/null || true

echo ""
echo "Test completed. Manual testing required for keyboard input verification."
echo "Run: make run"
echo "Then type 'abc' to verify all three characters are received."