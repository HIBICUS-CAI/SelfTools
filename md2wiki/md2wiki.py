import sys
import os
import re

def convert_file(in_path: str, out_path: str):
    print(in_path)
    print("↓...")
    print(out_path)

    with open(in_path, 'r', encoding="utf-8") as f:
        lines = f.readlines()

    all_blocks = []

    convert_md_title = ("# ", "## ", "### ")

    i = 0
    while i < len(lines):
        all_blocks.append([lines[i]])
        if not lines[i].startswith(convert_md_title):
            while len(lines) > (i + 1) and not lines[i + 1].startswith(convert_md_title):
                i += 1
                all_blocks[-1].append(lines[i])
        i += 1

    for block in all_blocks:
        if len(block) == 1 and block[0].startswith(convert_md_title):
            block[0] = block[0][block[0].index(' '):].rjust(len(block[0]), '*')
            title_hash_pattern = re.compile(r"^.*<!--hash:(?P<hash>.*)-->\n$")
            search_title_hash = re.search(title_hash_pattern, block[0])
            if search_title_hash is not None:
                block[0] = block[0][:block[0].index("<!--hash:")] + "[#{}]".format(search_title_hash.group("hash")) + "\n"
        else:
            block.insert(0, "&markdown_block_start{{\n")
            block.append("}}\n" if block[-1].endswith("\n") else "\n}}")
            i = 0
            is_now_fswiki = False
            while i < len(block):
                if block[i] == "```fswiki\n":
                    block[i] = "}}\n\n"
                    is_now_fswiki = True
                elif block[i] == "```\n" and is_now_fswiki:
                    block[i] = "\n&markdown_block_start{{\n"
                    is_now_fswiki = False
                i += 1

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    with open(out_path, 'w', encoding="utf-8") as output:
        for block in all_blocks:
            for line in block:
                output.write(line)

    print("done.")

def convert_dir(in_path: str):
    all_md_files = []
    for path, subdirs, files in os.walk(in_path):
        md_list = (name for name in files if name.endswith(".md"))
        for name in md_list:
            all_md_files.append(os.path.join(path, name))

    for md_path in all_md_files:
        out_path = os.path.join(in_path, "OUT_MD2WIKI", md_path[len(in_path):]) + ".2wiki.txt"
        convert_file(md_path, out_path)
        
in_path = sys.argv[1]
if os.path.isdir(in_path):
    convert_dir(os.path.join(os.path.abspath(in_path), ''))
elif in_path.endswith(".md"):
    convert_file(in_path, in_path + ".2wiki.txt")