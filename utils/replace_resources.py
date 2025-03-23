import os
import shutil
import sys

def copy_resources(resource_list, resource_dir, chromium_dir):
    if not os.path.isfile(resource_list):
        print(f"Resource list '{resource_list}' does not exist.")
        sys.exit(1)
    if not os.path.isdir(resource_dir):
        print(f"Resource dir '{resource_dir}' does not exist.")
        sys.exit(1)
    if not os.path.isdir(chromium_dir):
        print(f"Chromium dir '{chromium_dir}' does not exist.")
        sys.exit(1)

    with open(resource_list, 'r') as file:
        for line_number, line in enumerate(file, start=1):
            line = line.strip()
            if not line or line.startswith('#'):
                continue

            line_parts = line.split()
            source = os.path.join(resource_dir, line_parts[0])
            dest = os.path.join(chromium_dir, line_parts[1])

            if not os.path.exists(source):
                print(f"Source file '{source}' does not exist. Skipping copy.")
                continue
            if not os.path.exists(dest):
                print(f"Destination file '{dest}' does not exist. Skipping copy.")
                continue

            try:
                shutil.copyfile(source, dest)
                print(f"Copied {line.split()[0]}")
            except Exception as e:
                print(f"Error copying '{source}' to '{dest}': {e}")

def main():
    if len(sys.argv) != 4:
        print("Usage: python3 replace_resources.py <helium_resources.txt> <resources_dir> <chromium_src_dir>")
        sys.exit(1)

    resource_list = sys.argv[1]
    resource_dir = sys.argv[2]
    chromium_dir = sys.argv[3]

    copy_resources(resource_list, resource_dir, chromium_dir)

if __name__ == "__main__":
    main()
