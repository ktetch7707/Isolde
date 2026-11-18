from pathlib import Path

p = Path('src/media/KodiService.cpp')
content = p.read_text()

# Find the start of KodiService constructor
start_idx = content.find('KodiService::KodiService(const QString& profile, QObject* parent)')
if start_idx == -1:
    print("Could not find KodiService constructor")
else:
    print(f"Found KodiService::KodiService at index {start_idx}")
    # Find the end of constructor - look for the matching closing brace
    # Find opening brace
    brace_start = content.find('{', start_idx)
    print(f"Opening brace at {brace_start}")
    
    # Count braces to find matching close
    depth = 0
    brace_end = brace_start
    for i in range(brace_start, len(content)):
        if content[i] == '{':
            depth += 1
        elif content[i] == '}':
            depth -= 1
            if depth == 0:
                brace_end = i + 1
                break
    
    print(f"Closing brace at {brace_end}")
    print(f"Constructor body length: {brace_end - brace_start}")
    
    # Show what's in there
    print("First 200 chars of constructor body:")
    print(repr(content[brace_start+1:brace_start+201]))
