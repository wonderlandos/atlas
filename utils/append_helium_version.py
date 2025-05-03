import argparse
from pathlib import Path

CHROME_VERSION_BASE = 136

def get_version_part(path):
    with open(path, "r") as file:
        return int(file.readline().split(".")[0].strip())

def append_version(file, name, version_file_path, delta):
    version = get_version_part(version_file_path) + delta
    file.write(f"{name}={version}\n")

def check_existing_version(path):
    with open(path, "r") as f:
        if "HELIUM" in f.read():
            raise Exception("file already contains helium versioning")

def parse_args():
    parser = argparse.ArgumentParser()
    parser.add_argument("--tree", type=Path, required=True)
    parser.add_argument("--platform-tree", type=Path, required=True)
    parser.add_argument("--chromium-tree", type=Path, required=True)
    args = parser.parse_args()
    return (args.tree, args.platform_tree, args.chromium_tree)

def main():
    tree, platform_tree, chromium_tree = parse_args()
    version_parts = {
        "HELIUM_MAJOR": tree / "version.txt",
        "HELIUM_MINOR": tree / "chromium_version.txt",
        "HELIUM_PLATFORM": platform_tree / "revision.txt",
        "HELIUM_PATCH": tree / "revision.txt",
    }

    chrome_version_path = chromium_tree / "chrome/VERSION"
    check_existing_version(chrome_version_path)

    with open(chrome_version_path, "a") as f:
        for name, path in version_parts.items():
            delta = 0 if name != "HELIUM_MINOR" else -CHROME_VERSION_BASE
            append_version(f, name, path, delta)

if __name__ == "__main__":
    main()
