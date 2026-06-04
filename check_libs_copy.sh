#!/usr/bin/env bash

# --- Configuration ---
DEFAULT_LISTS=("/rootfs-pkgs.txt" "/desktopfs-pkgs.txt")

if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_executable> [destination_folder]"
    exit 1
fi

TARGET="$1"
DEST_DIR="$2"

if [ ! -r "$TARGET" ]; then
    echo "Error: Cannot read target file '$TARGET'"
    exit 1
fi

# If a second argument is given, ensure it's a valid directory
if [ -n "$DEST_DIR" ]; then
    if [ ! -d "$DEST_DIR" ]; then
        echo "Error: Destination '$DEST_DIR' is not a directory or does not exist."
        exit 1
    fi
fi

echo "Loading system default definitions..."

# Create hash map of original packages, discarding version strings
declare -A original_pkgs
for list_file in "${DEFAULT_LISTS[@]}"; do
    if [ -f "$list_file" ]; then
        while read -r pkg_name rest_of_line; do
            if [ -n "$pkg_name" ]; then
                original_pkgs["$pkg_name"]=1
            fi
        done < "$list_file"
    fi
done

echo "Analyzing dependencies for: $TARGET"
echo "===================================================================="

# Arrays for categorization
list_default=()
list_explicit=()
list_dependency=()
list_custom=()
list_missing=()

# Array to collect paths that need copying
copy_queue=()

# Process ldd output
while read -r line; do
    if [[ "$line" =~ =\>\ ([^[:space:]]+) ]]; then
        lib_path="${BASH_REMATCH[1]}"
        
        # Determine if the file is tracked by pacman
        if pkg_owner=$(pacman -Qqo "$lib_path" 2>/dev/null); then
            
            # Check if package belongs to the base image profile
            if [[ -n "${original_pkgs[$pkg_owner]}" ]]; then
                list_default+=( "$lib_path (Pkg: $pkg_owner)" )
            else
                # Non-default packages get flagged for copying
                if pacman -Qqe "$pkg_owner" &>/dev/null; then
                    list_explicit+=( "$lib_path (Pkg: $pkg_owner)" )
                else
                    list_dependency+=( "$lib_path (Pkg: $pkg_owner)" )
                fi
                copy_queue+=( "$lib_path" )
            fi
        else
            # Untracked files get flagged for copying
            list_custom+=( "$lib_path" )
            copy_queue+=( "$lib_path" )
        fi
        
    elif [[ "$line" =~ "not found" ]]; then
        list_missing+=( "$line" )
    fi
done < <(ldd "$TARGET" 2>/dev/null)

# --- Output Section ---

print_list() {
    local title="$1"
    local color="$2"
    shift 2
    local items=("${@}")
    
    echo -e "\n${color}${title}\e[0m"
    if [ ${#items[@]} -eq 0 ]; then
        echo "  None"
    else
        for item in "${items[@]}"; do
            echo "  - $item"
        done
    fi
}

print_list "[✔] SYSTEM DEFAULT (Included in Manjaro ISO)" "\e[32m" "${list_default[@]}"
print_list "[👤] USER EXPLICIT (Manually installed by you)" "\e[34m" "${list_explicit[@]}"
print_list "[🔗] USER DEPENDENCY (Pulled in by your apps)" "\e[35m" "${list_dependency[@]}"
print_list "[🛠️] CUSTOM / UNKNOWN (Not in pacman)" "\e[31m" "${list_custom[@]}"

if [ ${#list_missing[@]} -ne 0 ]; then
    print_list "[⚠] MISSING LIBRARIES" "\e[1;31m" "${list_missing[@]}"
fi

echo -e "\n===================================================================="

# --- Optional File Copy Engine ---
if [ -n "$DEST_DIR" ]; then
    if [ ${#copy_queue[@]} -eq 0 ]; then
        echo -e "\n[\e[32mℹ\e[0m] No user or custom libraries found to copy."
    else
        echo -e "\n[\e[33m🚀\e[0m] Copying ${#copy_queue[@]} files to: $DEST_DIR"
        
        for file in "${copy_queue[@]}"; do
            # Resolve symbolic links so you copy the actual binary file data
            real_file=$(realpath "$file" 2>/dev/null || echo "$file")
            
            if [ -f "$real_file" ]; then
                if cp "$real_file" "$DEST_DIR/"; then
                    echo "  -> Copied: $(basename "$real_file")"
                else
                    echo "  -> [ERROR] Failed to copy: $real_file"
                fi
            else
                echo "  -> [ERROR] File does not exist: $real_file"
            fi
        done
        echo -e "\n[\e[32m✔\e[0m] Copy operations complete."
    fi
    echo "===================================================================="
fi
