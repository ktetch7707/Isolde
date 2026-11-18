from pathlib import Path
p = Path('src/app/AppRegistry.cpp')
lines = p.read_text().splitlines(keepends=True)

# Find appIcon end (around line 93, index 92)
# Look for the leftover code from old resolvePackagedIcon
# Find line 94-105 (indices 93-104) which has leftover code
start = None
for i, ln in enumerate(lines):
    if 'return false;' in ln and i > 90 and i < 100:
        # Check if this is the leftover code after appIcon
        # Line 95 should be `return false;`
        if i + 5 < len(lines) and 'excluded.append(colour)' in lines[i+5]:
            start = i
            break

if start is None:
    print("Could not find leftover code")
else:
    # Find end of leftover code
    end = start
    for i in range(start, len(lines)):
        if lines[i].strip() == '}':
            end = i + 1
            break
    print(f'Leftover code spans lines {start+1} to {end}')
    print(f'Lines:')
    for j in range(start, min(end, start+20)):
        print(f'  {j+1}:{lines[j]}', end='')
    new_lines = lines[:start] + lines[end:]
    p.write_text(''.join(new_lines))
    print(f'Removed leftover code. Total lines: {len(new_lines)}')