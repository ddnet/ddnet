#!/usr/bin/env python3
# TODO: Please remove this when we switch completely to ruff.
# pylint: disable=bad-indentation
import argparse
import csv
from pathlib import Path

import twlang


def main() -> None:
    parser = argparse.ArgumentParser(description="Convert DDNet translation files into Weblate CSV files in the specified directory.")
    parser.add_argument(
        "out_dir",
        metavar="OUT_DIR",
        help="Directory to output the Weblate CSV files in",
    )
    args = parser.parse_args()

    prefix = Path(__file__).resolve().parent.parent

    for key, (_, _, _, raw_rfc3066, _) in sorted(twlang.language_index().items()):
        rfc3066 = raw_rfc3066.split(";")[0]
        filename = f"{prefix}/data/languages/{key}.txt"
        with Path(f"{args.out_dir}/{rfc3066}.csv").open("w", encoding="utf-8", newline="\r\n") as f:
            # Needs to use the "unix" dialect, otherwise Weblate misdetects
            # space as field separator in some of the translation files.
            writer = csv.DictWriter(f, ["source", "context", "target"], dialect="unix")
            writer.writeheader()
            for (source, context), (_, translation, _) in twlang.translations(filename).items():
                writer.writerow(
                    {
                        "source": source,
                        "context": context,
                        "target": translation,
                    }
                )


if __name__ == "__main__":
    main()
