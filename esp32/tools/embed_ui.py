from pathlib import Path
root=Path(__file__).resolve().parents[1]
html=(root/'data/index.html').read_text(encoding='utf-8')
assert ')KT2HTML"' not in html
(root/'src/generated_ui.h').write_text('#pragma once\nstatic const char INDEX_HTML[] PROGMEM = R"KT2HTML('+html+')KT2HTML";\n',encoding='utf-8')
print('UI embedded without changing the user data filesystem')
