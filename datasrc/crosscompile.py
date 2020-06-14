def get_msgs():
    from datatypes import NetMessage
    import network

    return ["NETMSG_INVALID"] + [m.enum_name for m in network.Messages]

def get_msgs_7():
    from seven.datatypes import NetMessage
    import seven.network as network

    return ["NETMSG_INVALID"] + [m.enum_name for m in network.Messages]

def get_objs():
    from datatypes import NetObject
    import network

    return ["NETOBJ_INVALID"] + [m.enum_name for m in network.Objects if m.ex is None]

def get_objs_7():
    from seven.datatypes import NetObject
    import seven.network as network

    return ["NETOBJ_INVALID"] + [m.enum_name for m in network.Objects]

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
    print("inline int {0}(int a) {{ if(a < 0 || a >= {1}) return -1; return gs_{0}[a]; }}".format(name, len(map)))

def main():
    guard = "GAME_GENERATED_PROTOCOLGLUE"
    print("#ifndef " + guard)
    print("#define " + guard)

    msgs = get_msgs()
    msgs7 = get_msgs_7()

    output_map("Msg_SixToSeven", generate_map(msgs, msgs7))
    output_map("Msg_SevenToSix", generate_map(msgs7, msgs))

    objs = get_objs()
    objs7 = get_objs_7()
    output_map("Obj_SixToSeven", generate_map(objs, objs7))
    output_map("Obj_SevenToSix", generate_map(objs7, objs))

    print("#endif //" + guard)

if __name__ == "__main__":
    main()
