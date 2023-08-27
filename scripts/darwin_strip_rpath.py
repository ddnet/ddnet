import argparse
import subprocess
import re

def split_cmds(lines):
    cmds = []
    current = []
    load_cmd_regex = re.compile(r"^Load command \d+$")
    for line in lines:
        if load_cmd_regex.match(line):
            cmds.append(current)
            current = []
            continue

        current.append(line.strip())

    return cmds[1:]

def main():
    p = argparse.ArgumentParser(description="Удаление команд LC_RPATH из исполняемого файла")

    p.add_argument('otool', help="Путь к утилите otool")
    p.add_argument('install_name_tool', help="Путь к утилите install_name_tool")
    p.add_argument('executable', metavar="ИСПОЛНЯЕМЫЙ_ФАЙЛ", help="Исполняемый файл для обработки")
    args = p.parse_args()

    otool = args.otool
    install_name_tool = args.install_name_tool
    executable = args.executable

    try:
        output = subprocess.check_output([otool, "-l", executable], stderr=subprocess.STDOUT)
    except subprocess.CalledProcessError as e:
        print("Ошибка при выполнении otool:", e.output.decode())
        return

    cmds = split_cmds(output.decode().splitlines())
    lc_rpath_cmds = [cmd for cmd in cmds if cmd[0] == "cmd LC_RPATH"]

    path_regex = re.compile(r"^path (.*) \(offset \d+\)$")
    rpaths = {path_regex.match(part).group(1) for cmd in lc_rpath_cmds for part in cmd if path_regex.match(part)}

    print("Найденные пути:")
    for path in rpaths:
        print("\t" + path)
        try:
            subprocess.check_call([install_name_tool, "-delete_rpath", path, executable])
            print(f"Удален путь {path} из {executable}")
        except subprocess.CalledProcessError as e:
            print("Ошибка при выполнении install_name_tool:", e)

if __name__ == '__main__':
    main()
