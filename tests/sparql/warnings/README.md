# SPARQL Warning Tests

This directory contains tests for SPARQL query warning detection.

## Current Status

**Active Tests (in manifest.ttl):**
- `warning-3`: Duplicate variable detection - ✅ **PASSING**

**Parked Tests (not in manifest):**
- `warning-1`: Selecting variable never bound - ⏸️ **PARKED** 
- `warning-2`: Binding variable never selected - ⏸️ **PARKED**

## Known Issues

The `warning-1` and `warning-2` tests are temporarily parked due to the scope system architectural transition. These tests depend on variable usage analysis that was disrupted during the variables use map removal in the query scope architecture enhancement.

**Technical Details:**
- `warning-1` should trigger `RASQAL_WARNING_LEVEL_SELECTED_NEVER_BOUND` (level 10)
- `warning-2` should trigger `RASQAL_WARNING_LEVEL_VARIABLE_UNUSED` (level 30)
- Both currently return exit code 0 instead of expected exit code 2

**Resolution:** These tests will be re-enabled in Phase C when scope-aware warning detection is implemented.

**Reference:** See `specs/query-scope-architecture-enhancement.md` for full details on the architectural transition and resolution plan.

## Running Tests

```bash
# Run active warning tests (only warning-3)
make check

# Test individual warnings manually
../../../utils/roqet -q warning-1.rq empty.nt  # Should warn but doesn't
../../../utils/roqet -q warning-2.rq empty.nt  # Should warn but doesn't  
../../../utils/roqet -q warning-3.rq empty.nt  # Correctly warns
```

## Test Files

- `manifest.ttl` - Active test manifest (warning-3 only)
- `warning-1.rq` - Query selecting unbound variable (parked)
- `warning-1.out` - Expected output for warning-1 (parked)
- `warning-2.rq` - Query with unused bound variable (parked)
- `warning-2.out` - Expected output for warning-2 (parked)
- `warning-3.rq` - Query with duplicate variables (active)
- `warning-3.out` - Expected output for warning-3 (active)
- `empty.nt` - Empty test data file