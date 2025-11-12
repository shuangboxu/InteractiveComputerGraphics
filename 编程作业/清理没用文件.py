## 警告：运行本脚本将自动备份当前目录后递归删除匹配的文件和文件夹（不可恢复），请确认目录无误后再执行
# 作者：许双博 
# 时间：2025年10月
import os
import shutil
import time

# 当前脚本所在目录作为根目录
root_dir = os.getcwd()

# 生成备份文件名（存放在同级目录）
timestamp = time.strftime("%Y%m%d_%H%M%S")
backup_name = f"{os.path.basename(root_dir)}_backup_{timestamp}.zip"
backup_path = os.path.join(os.path.dirname(root_dir), backup_name)

# 创建备份
print(f"Creating backup of current folder:\n{root_dir}")
print(f"Backup file: {backup_path}\n")

try:
    shutil.make_archive(backup_path[:-4], 'zip', root_dir)
    print("[OK] Backup created successfully.")
except Exception as e:
    print(f"[ERROR] Failed to create backup: {e}")
    exit(1)

# 要删除的文件夹名（模糊匹配）
folders_to_delete = [
    ".vs", "x64", "Debug", "Release", "VS2019_Win10", "ipch"
]

# 要删除的文件扩展名
extensions_to_delete = [
    ".obj", ".pdb", ".idb", ".ilk", ".tlog", ".log",
    ".pch", ".suo", ".db", ".db-shm", ".db-wal",
    ".recipe", ".exe", ".bak", ".opendb", ".lastbuildstate"
]

print(f"\nCleaning Visual Studio cache files under:\n{root_dir}\n")

deleted_files = 0
deleted_dirs = 0

for dirpath, dirnames, filenames in os.walk(root_dir, topdown=False):
    # 删除指定文件夹
    for d in dirnames:
        if any(key.lower() in d.lower() for key in folders_to_delete):
            full_path = os.path.join(dirpath, d)
            try:
                shutil.rmtree(full_path)
                deleted_dirs += 1
                print(f"[DIR] Removed {full_path}")
            except Exception as e:
                print(f"Failed to remove folder {full_path}: {e}")

    # 删除指定后缀文件
    for f in filenames:
        if any(f.lower().endswith(ext) for ext in extensions_to_delete):
            full_path = os.path.join(dirpath, f)
            try:
                os.remove(full_path)
                deleted_files += 1
                print(f"[FILE] Removed {full_path}")
            except Exception as e:
                print(f"Failed to remove file {full_path}: {e}")

print("\nCleanup completed.")
print(f"Backup saved as: {backup_path}")
print(f"Deleted {deleted_files} files and {deleted_dirs} folders.")
