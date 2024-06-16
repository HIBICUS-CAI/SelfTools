import sys

with open(sys.argv[1], 'r', encoding="utf-8") as f:
    lines = f.readlines()

all_blocks = [[]]
all_blocks.clear()

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
        print(block[0].rstrip())
    else:
        block.insert(0, "&markdown_block_start{{\n")
        block.append("}}\n")
        for text in block:
            print(text.rstrip())

with open("D:\code\SelfTools\md2wiki\somefile.txt", 'w', encoding="utf-8") as output:
    for block in all_blocks:
        for line in block:
            output.write(line)