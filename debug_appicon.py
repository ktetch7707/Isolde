from pathlib import Path
p = Path('src/app/AppRegistry.cpp')
lines = p.read_text().splitlines(keepends=True)
print(f'Total lines: {len(lines)}')
print(f'Line 50: {repr(lines[49])}')
print(f'Line 51: {repr(lines[50])}')
print(f'Line 55: {repr(lines[54])}')
print(f'Line 123: {repr(lines[122])}')