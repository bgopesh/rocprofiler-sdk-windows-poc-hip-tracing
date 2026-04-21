# Mock Tool 2 - Multi-Tool Testing

A second mock profiling tool for testing multi-tool initialization and verifying sequential call order.

## Purpose

This tool is used to verify:
1. **Multi-tool support**: Both tools can be loaded simultaneously
2. **Call order**: Tools are initialized in ROCP_TOOL_LIBRARIES order
3. **No race conditions**: Each tool's configure is called sequentially, not concurrently
4. **Thread safety**: All calls happen on the same thread

## Differences from Mock Tool 1

- **Distinct logging**: Prefixed with `[MOCK TOOL 2]`
- **Call order tracking**: Global atomic counter shows call sequence
- **Thread ID logging**: Verifies single-threaded initialization
- **Initialization delay**: 100ms sleep to expose potential race conditions
- **Timestamp logging**: Nanosecond precision to verify sequential calls

## Usage

### Load Both Tools

```bash
# Windows
set ROCP_TOOL_LIBRARIES=mock-tool.dll;mock-tool-2.dll
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe

# Or with absolute paths
set ROCP_TOOL_LIBRARIES=C:\path\to\mock-tool.dll;C:\path\to\mock-tool-2.dll
```

### Expected Output

```
[MOCK TOOL] rocprofiler_configure called
  Call Order: 0
  Thread ID: 12345
  Timestamp: 123456789000000

[MOCK TOOL 2] rocprofiler_configure called
  Call Order: 1
  Thread ID: 12345  (same thread)
  Timestamp: 123456889000000  (100ms+ later)
```

## Verification Points

### ✅ Sequential Execution
- Call Order: 0, 1, 2, ... (no gaps, no duplicates)
- Timestamps: Each call starts after previous completes

### ✅ Same Thread
- All Thread IDs identical (no concurrent calls)

### ✅ Order Matches ROCP_TOOL_LIBRARIES
- First tool in list → Call Order 0
- Second tool in list → Call Order 1

### ❌ Race Condition (should NOT happen)
- Would show: Call Order 0, 0, 1 (duplicates)
- Would show: Different Thread IDs
- Would show: Overlapping timestamps

## Building

Included automatically in root CMakeLists.txt:

```bash
cd build
cmake -G "Visual Studio 17 2022" -A x64 ..
cmake --build . --config Release
```

Output: `build/mock-tool-2/Release/mock-tool-2.dll`

## Testing

### Test 1: Single Tool (Control)
```bash
set ROCP_TOOL_LIBRARIES=mock-tool-2.dll
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
```

Expected: Tool 2 called once with Call Order 0

### Test 2: Two Tools (Order A)
```bash
set ROCP_TOOL_LIBRARIES=mock-tool.dll;mock-tool-2.dll
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
```

Expected:
- Tool 1: Call Order 0
- Tool 2: Call Order 1

### Test 3: Two Tools (Order B - Reversed)
```bash
set ROCP_TOOL_LIBRARIES=mock-tool-2.dll;mock-tool.dll
python rocprofv3-poc.py ./build/test-hip-app/Release/test-hip-app.exe
```

Expected:
- Tool 2: Call Order 0
- Tool 1: Call Order 1

## Implementation Notes

### Thread Safety
- Uses `std::atomic<int>` for call counter (thread-safe increment)
- No mutexes needed - register calls tools sequentially

### Initialization Delay
- 100ms `sleep_for()` simulates real tool initialization
- Exposes race conditions if present (overlapping execution)
- Does NOT affect correctness (just testing tool)

### Global Counter
- Shared across ALL tools that use this pattern
- Demonstrates call order across multiple DLLs
- Would show race if calls were concurrent

## Code Structure

```cpp
// Global state
std::atomic<int> g_call_count{0};  // Shared counter

// Entry point
TOOL_API void* rocprofiler_configure(...) {
    int my_order = g_call_count.fetch_add(1);  // Atomic increment
    printf("Call Order: %d\n", my_order);
    
    // Simulate work
    sleep_for(100ms);
    
    return nullptr;
}
```

## Expected Behavior

### Correct (Sequential)
```
Time ──────────────────────────────────────────>
       │←─ Tool 1 ─→│←─ Tool 2 ─→│
       0ms         100ms        200ms
Thread: ████████████████████████████
```

### Incorrect (Concurrent - should NOT happen)
```
Time ──────────────────────────────────────────>
       │←─ Tool 1 ─→│
       │←─ Tool 2 ─→│
       0ms         100ms
Thread 1: ████████████
Thread 2: ████████████  ← Race condition!
```

The implementation ensures the first scenario (sequential).
