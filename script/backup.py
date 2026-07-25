import os
import tarfile
from datetime import datetime
from pathlib import Path
from urllib.parse import urlparse
import shutil
from typing import Tuple
from smbprotocol.open import CreateDisposition
from smbclient import register_session, ClientConfig
import smbclient
import getpass
from tqdm import tqdm
from dotenv import load_dotenv

"""Creates a local tarball archive of the project and pushes it to the NAS."""


class ProgressFileObject:
    def __init__(self, filename: Path, pbar: tqdm):
        self._filename = filename
        self._pbar = pbar
        self._file = open(filename, "rb")
        self._size = os.path.getsize(filename)
        self._pbar.total = self._size
        self._pbar.refresh()

    def read(self, size: int) -> bytes:
        data = self._file.read(size)
        self._pbar.update(len(data))
        return data

    def close(self):
        self._file.close()

    def __enter__(self):
        return self

    def __exit__(self, exc_type, exc_val, exc_tb):
        self.close()


def find_project_root(script_path: Path) -> Tuple[Path, str]:
    """
    Find the project root by looking for common project markers.
    Returns tuple of (root_path, project_name).
    
    Looks for directories that typically indicate a project root:
    - .git (version control)
    - build (build artifacts)
    - source or src (source code)
    """
    current = script_path.resolve()
    project_markers = {'.git', 'build', 'source', 'src'}
    
    # Walk up the directory tree
    while current.parent != current:  # Stop at filesystem root
        # Check if any marker exists in current directory
        if any((current / marker).exists() for marker in project_markers):
            return current, current.name
        current = current.parent
    
    # If we reach here, we couldn't find a project root
    raise RuntimeError(
        f"Could not find project root. Looking for one of: {', '.join(project_markers)}"
    )


def should_exclude(path: Path, exclude_patterns: set) -> bool:
    """
    Check if a path should be excluded from backup.
    Handles both root-level directories and nested paths.
    """
    # Convert path to string for pattern matching
    path_str = str(path)

    # Check if the path matches any exclusion pattern
    for pattern in exclude_patterns:
        # Convert backslashes to forward slashes for consistency
        norm_path = path_str.replace("\\", "/")
        norm_pattern = pattern.replace("\\", "/")

        # Check for exact directory name match or nested path match
        if (
            norm_pattern in path.parts  # Matches directory name anywhere
            or norm_pattern in norm_path  # Matches part of the path
            or path.name == norm_pattern
        ):  # Matches exact name
            return True

    return False


def calculate_total_size(directory: Path, exclude_patterns: set) -> int:
    """Calculate total size of files to be backed up."""
    total_size = 0
    for root, dirs, files in os.walk(directory):
        rel_root = Path(root).relative_to(directory)

        # Filter directories
        dirs[:] = [
            d for d in dirs if not should_exclude(rel_root / d, exclude_patterns)
        ]

        # Add up file sizes
        for file in files:
            file_path = Path(root) / file
            if not should_exclude(rel_root / file, exclude_patterns):
                total_size += file_path.stat().st_size
    return total_size


def create_backup(follow_symlinks: bool = True) -> Path:
    """Create a backup of the project excluding specified directories."""
    # Get script location and project root
    script_path = Path(__file__).parent
    project_root, project_name = find_project_root(script_path)
    
    print(f"Found project root: {project_root}")
    print(f"Project name: {project_name}")

    # Define exclusions
    exclude_patterns = {
        "build",
        ".git",
        ".cache",
        "source/vendor",
        "scripts/__pycache__",
        "__pycache__",
        # ".vscode",
        # ".vs",
        "node_modules",
    }

    # Calculate total size for progress bar
    total_size = calculate_total_size(project_root, exclude_patterns)

    # Generate archive name with current date and project name
    date_str = datetime.now().strftime("%Y%m%d")
    archive_name = f"{project_name}-{date_str}.tar"
    archive_path = project_root / archive_name

    # Create tarfile
    with tqdm(
        total=total_size, unit="B", unit_scale=True, desc="Creating archive"
    ) as pbar:
        with tarfile.open(archive_path, "w") as tar:
            for root, dirs, files in os.walk(project_root, followlinks=follow_symlinks):
                rel_root = Path(root).relative_to(project_root)

                dirs[:] = [
                    d
                    for d in dirs
                    if not should_exclude(rel_root / d, exclude_patterns)
                ]

                for file in files:
                    if file == archive_name:
                        continue

                    file_path = Path(root) / file
                    arc_path = rel_root / file

                    if not should_exclude(arc_path, exclude_patterns):
                        tar.add(file_path, arcname=arc_path)
                        pbar.update(file_path.stat().st_size)

    return archive_path


def parse_smb_url(url: str) -> Tuple[str, str, str]:
    """
    Parse SMB URL into components.
    Returns (server, share, path)
    """
    parsed = urlparse(url)
    if parsed.scheme != "smb":
        raise ValueError(f"Invalid backup path scheme: {parsed.scheme}. Expected 'smb'")

    # Get server from netloc
    server = parsed.netloc

    # Split path into share and remaining path
    parts = parsed.path.strip("/").split("/", 1)
    if not parts:
        raise ValueError("Invalid SMB URL: no share specified")

    share = parts[0]
    path = parts[1] if len(parts) > 1 else ""

    return server, share, path


def get_smb_credentials() -> Tuple[str, str]:
    """Get SMB credentials from environment or prompt user."""
    username = os.getenv("BACKUP_SMB_USER")
    password = os.getenv("BACKUP_SMB_PASS")

    if not username:
        username = input("SMB Username: ")
    if not password:
        password = getpass.getpass("SMB Password: ")

    return username, password


def move_to_nas(archive_path: Path) -> None:
    """
    Move the backup archive to the NAS location using authenticated SMB connection.
    Shows progress during upload.
    """
    backup_path = os.getenv("BACKUP_REMOTE_PATH")
    if not backup_path:
        raise EnvironmentError("BACKUP_REMOTE_PATH environment variable is not set")

    server, share, remote_path = parse_smb_url(backup_path)
    username, password = get_smb_credentials()

    try:
        smbclient.ClientConfig(username=username, password=password)

        remote_file = f"//{server}/{share}"
        if remote_path:
            remote_file = f"{remote_file}/{remote_path}"
        remote_file = f"{remote_file}/{archive_path.name}"

        print(f"Starting upload to {remote_file}...")

        with tqdm(
            total=archive_path.stat().st_size,
            unit="B",
            unit_scale=True,
            desc="Uploading to NAS",
        ) as pbar:
            with ProgressFileObject(archive_path, pbar) as src:
                with smbclient.open_file(remote_file, mode="wb") as dst:
                    shutil.copyfileobj(src, dst, length=1024 * 1024)  # 1MB chunks

        archive_path.unlink()
        print(f"Backup moved to: {remote_file}")

    except Exception as e:
        raise RuntimeError(f"Failed to copy to SMB share: {str(e)}")
    finally:
        smbclient.reset_connection_cache()


if __name__ == "__main__":
    # Load environment variables from .env
    load_dotenv()
    
    try:
        # Create the backup archive
        print("Creating archive")
        archive_path = create_backup()
        print(f"Archive created successfully: {archive_path}")

        # Move to NAS
        try:
            move_to_nas(archive_path)
            print("Backup process completed successfully")
        except Exception as e:
            print(f"Error moving backup to NAS: {e}")
            # Clean up the archive if we couldn't move it
            if archive_path.exists():
                archive_path.unlink()
            raise

    except Exception as e:
        print(f"Error during backup process: {e}")