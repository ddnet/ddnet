SIZES = "8 16 32 64 size".split()
TYPES = [signedness + size for size in SIZES for signedness in "iu"]

def formatt(string, type):
    trait = type[0].upper() + type[1:]
    return string.format(**{
        "type": type,
        "trait": trait,
        "ntrait": "N" + trait,
    })

def printt(string, type):
    print(formatt(string, type))
