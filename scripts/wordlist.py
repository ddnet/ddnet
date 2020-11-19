print("#ifndef GENERATED_WORDLIST_H")
print("#define GENERATED_WORDLIST_H")
print("const char g_aFallbackWordlist[][32] = {")
with open("data/wordlist.txt") as f:
    for line in f:
        word = line.strip().split("\t")[1]
        print("\t\"%s\", " % word)
print("};")
print("#endif // GENERATED_WORDLIST_H")
