def get_msgs():
    from datatypes import NetMessage
    import network

    return ["NETMSG_INVALID"] + [m.enum_name for m in network.Messages]

def get_msgs_7():
    from seven.datatypes import NetMessage
    import seven.network as network

    return ["NETMSG_INVALID"] + [m.enum_name for m in network.Messages]

def generate_map(a, b):
    map = []
    for i, m in enumerate(a):
        try:
            map += [b.index(m)]
        except ValueError:
            map += [-1]

    return map


def output_map(name, map):
    print("static const int gs_{}[{}] = {{".format(name, len(map)))
    print(*map, sep=',')
    print("};")
    print("inline int {0}(int a) {{ return gs_{0}[a]; }}".format(name))

def main():
    msgs = get_msgs()
    msgs7 = get_msgs_7()

    output_map("SixToSeven", generate_map(msgs, msgs7))
    output_map("SevenToSix", generate_map(msgs7, msgs))

if __name__ == "__main__":
    main()
