import re

files = [
    'C:/Users/kketc/Documents/ChatGPT/Isolde/src/games/GameLibrary.h',
    'C:/Users/kketc/Documents/ChatGPT/Isolde/src/games/GameLibrary_collections.cpp',
    'C:/Users/kketc/Documents/ChatGPT/Isolde/src/media/KodiMedia.cpp',
    'C:/Users/kketc/Documents/ChatGPT/Isolde/src/media/KodiMedia.h',
]

return_types = {'bool', 'QString', 'QVariantMap', 'QVariantList', 'QJsonObject', 'void', 'int', 'qint64', 'QStringList', 'QSqlQuery', 'QVariant', 'QSet', 'QMap'}

for fpath in files:
    try:
        with open(fpath, 'r', encoding='utf-8-sig') as f:
            content = f.read()
    except FileNotFoundError:
        print(f'Not found: {fpath}')
        continue

    lines = content.split('\n')
    result = []
    i = 0
    count = 0

    while i < len(lines):
        line = lines[i]
        stripped = line.strip()

        # Check if this line is a return type followed by a function name on next line
        if stripped in return_types:
            # Look ahead to see if next line has a function name
            if i + 1 < len(lines):
                next_stripped = lines[i + 1].strip()
                func_match = re.match(r'^(\w+::\w+)\((.*)\)', next_stripped)
                if func_match:
                    # Check if previous non-empty line is already a doxygen comment closer
                    j = len(result) - 1
                    while j >= 0 and result[j].strip() == '':
                        j -= 1
                    has_doxygen = j >= 0 and result[j].strip().endswith(' */')

                    if not has_doxygen:
                        ret_type = stripped
                        func_name = func_match.group(1)
                        params_str = func_match.group(2)
                        func_short = func_name.split('::')[-1]

                        # Parse parameters
                        params = []
                        if params_str.strip() and params_str.strip() != 'void':
                            for p in params_str.split(','):
                                p = p.strip()
                                parts = p.split()
                                if len(parts) >= 2:
                                    pname = parts[-1]
                                    if not pname.startswith('_'):
                                        params.append(pname)

                        result.append('/**')
                        result.append(f' * @brief {func_short.replace("_", " ").title()}.')
                        result.append(f' * @details Implements {func_short} functionality.')
                        result.append(f' * @why Ensures {func_short} operates correctly within Isolde.')
                        for p in params:
                            result.append(f' * @param {p} The {p} parameter.')
                        if ret_type != 'void':
                            result.append(f' * @return {ret_type} result of the operation.')
                        result.append(' */')
                        count += 1

        result.append(line)
        i += 1

    with open(fpath, 'w', encoding='utf-8') as f:
        f.write('\n'.join(result))
    print(f'{fpath}: added {count} doxygen comments')