#!/usr/bin/env python3
import argparse
import os
import sqlite3
import sys


def parse_args():
    parser = argparse.ArgumentParser(description="Create sqlite db from SQL dump.")
    parser.add_argument("--sql", required=True, help="SQL dump path")
    parser.add_argument("--out", required=True, help="Output sqlite path")
    parser.add_argument("--force", action="store_true", help="Overwrite existing db")
    return parser.parse_args()


def main():
    args = parse_args()
    sql_path = os.path.abspath(args.sql)
    out_path = os.path.abspath(args.out)

    if not os.path.isfile(sql_path):
        print("missing sql:", sql_path, file=sys.stderr)
        return 1

    if os.path.exists(out_path) and not args.force:
        print("database exists, skip:", out_path)
        return 0

    os.makedirs(os.path.dirname(out_path), exist_ok=True)
    if os.path.exists(out_path):
        os.remove(out_path)

    with open(sql_path, "r", encoding="utf-8") as handle:
        script = handle.read()

    conn = sqlite3.connect(out_path)
    try:
        conn.executescript(script)
        conn.commit()
    finally:
        conn.close()

    print("created database:", out_path)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
