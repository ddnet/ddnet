# TODO: Please remove this when we switch completely to ruff.
# pylint: disable=bad-indentation
import functools
import os
import re
from collections import OrderedDict
from pathlib import Path
from typing import IO, Any

TranslationKey = tuple[str, str]
TranslationData = list[Any]
DecodedData = dict[TranslationKey, TranslationData]


class LanguageDecodeError(Exception):
    def __init__(self, message: str, filename: str, line: int) -> None:
        error = f'File "{filename}", line {line + 1}: {message}'
        super().__init__(error)


def decode(fileobj: IO[str], elements_per_key: int, *, allow_context: bool = True) -> DecodedData:  # noqa: PLR0912, C901
    data = {}
    current_context = ""
    current_key = None
    index = -1
    for index, raw_line in enumerate(fileobj):
        line = raw_line.encode("utf-8").decode("utf-8-sig").rstrip("\r\n")
        if not line or line[:1] == "#":
            current_context = ""
            continue

        if line[0] == "[":
            if line[-1] != "]":
                raise LanguageDecodeError("Invalid context string", fileobj.name, index)
            if not allow_context:
                raise LanguageDecodeError("Context not allowed in this file", fileobj.name, index)
            current_context = line[1:-1]
        elif line[:3] == "== ":
            if len(data[current_key]) >= 1 + elements_per_key:
                raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
            if current_key:
                translation = line[3:]
                data[current_key].extend([translation])
            else:
                raise LanguageDecodeError("Element before key given", fileobj.name, index)
        else:
            if current_key:
                if len(data[current_key]) != 1 + elements_per_key:
                    raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
                data[current_key].append(index - 1 if current_context else index)
            if line in data:
                raise LanguageDecodeError("Key defined multiple times: " + line, fileobj.name, index)
            data[(line, current_context)] = [index - 1 if current_context else index]
            current_key = (line, current_context)
    if len(data[current_key]) != 1 + elements_per_key:
        raise LanguageDecodeError("Wrong number of elements per key", fileobj.name, index)
    data[current_key].append(index + 1)
    return {key: value for key, value in data.items() if key[0]}


def check_file(path: Path) -> list[tuple]:
    with path.open(encoding="utf-8") as file:
        return re.findall(
            r"(Localize|Localizable)\s*\(\s*\"((?:(?:\\\")|[^\"])+)\"(?:\s*,\s*\"((?:(?:\\\")|[^\"])+)\")?\s*\)",
            file.read(),
        )


@functools.lru_cache(None)
def check_folder(path: str) -> OrderedDict[TranslationKey, None]:
    englishlist: OrderedDict[TranslationKey, None] = OrderedDict()

    for path2, dirs, files in os.walk(path):
        dirs.sort()
        for f in sorted(files):
            if not any(f.endswith(x) for x in [".cpp", ".c", ".h"]):
                continue
            for sentence in check_file(Path(path2) / f):
                key = (
                    sentence[1:][0].replace('\\"', '"'),
                    sentence[1:][1].replace('\\"', '"'),
                )
                englishlist[key] = None
    return englishlist


def language_index() -> dict[str, TranslationData]:
    with Path("data/languages/index.txt").open(encoding="utf-8") as f:
        return {key: value for (key, _), value in decode(f, 3, allow_context=False).items()}


def languages() -> dict[str, list[Any]]:
    index = language_index()
    return {"data/languages/" + key + ".txt": [key, *elements] for key, elements in index.items()}


def translations(filename: str) -> DecodedData:
    with Path(filename).open(encoding="utf-8") as f:
        return decode(f, 1)


def localizes() -> list[TranslationKey]:
    return list(check_folder("src"))
