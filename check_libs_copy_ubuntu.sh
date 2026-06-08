#!/usr/bin/env bash

# --- Ubuntu Configuration ---
# In Ubuntu, we don't have a single text file list like Manjaro, 
# so we'll treat everything that isn't in /usr/local or a custom path 
# as potentially "system" unless it's a known dependency we want to bundle.

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

# If a second argument is given, ensure it exists
if [ -n "$DEST_DIR" ] && [ ! -d "$DEST_DIR" ]; then
    echo "Error: Destination '$DEST_DIR' is not a directory."
    exit 1
fi

echo "Analyzing dependencies (Ubuntu Version) for: $TARGET"
echo "===================================================================="

# Arrays for categorization
list_system=()     # Provided by essential Ubuntu packages
list_app=()        # Libraries we should probably bundle
list_custom=()     # Not managed by dpkg
list_missing=()

# Array to collect paths that need copying
copy_queue=()

# Process ldd output
# ldd output format: "libname.so.1 => /lib/x86_64-linux-gnu/libname.so.1 (0x...)"
while read -r line; do
    if [[ "$line" =~ =\>\ ([^[:space:]]+) ]]; then
        lib_path="${BASH_REMATCH[1]}"
        
        real_lib_path=$(realpath "$lib_path" 2>/dev/null || echo "$lib_path")
        
        # Check if dpkg owns the file (try both resolved and original path due to usrmerge)
        if pkg_info=$(dpkg -S "$real_lib_path" 2>/dev/null || dpkg -S "$lib_path" 2>/dev/null); then
            pkg_name=$(echo "$pkg_info" | cut -d: -f1 | head -n 1)
            
            # Heuristic: Libraries in /lib or /usr/lib that belong to "standard" 
            # packages are often safe to skip in AppImages, but for safety 
            # in this project, we'll bundle things that aren't core libc/libpthread.
            
            if [[ "$lib_path" =~ /(libc|libpthread|libdl|libm|librt|libutil)\.so ]]; then
                list_system+=( "$lib_path (Pkg: $pkg_name) [SKIPPED]" )
            else
                list_app+=( "$lib_path (Pkg: $pkg_name) [BUNDLED]" )
                copy_queue+=( "$lib_path" )
            fi
        else
            # Untracked files (manually compiled)
            list_custom+=( "$lib_path [CUSTOM]" )
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

print_list "[✔] CORE SYSTEM (Skipped)" "\e[32m" "${list_system[@]}"
print_list "[📦] APP DEPENDENCIES (Bundled)" "\e[34m" "${list_app[@]}"
print_list "[🛠️] CUSTOM / UNKNOWN (Bundled)" "\e[31m" "${list_custom[@]}"

if [ ${#list_missing[@]} -ne 0 ]; then
    print_list "[⚠] MISSING LIBRARIES" "\e[1;31m" "${list_missing[@]}"
fi

echo -e "\n===================================================================="

# --- Optional File Copy Engine ---
if [ -n "$DEST_DIR" ]; then
    if [ ${#copy_queue[@]} -eq 0 ]; then
        echo -e "\n[\e[32mℹ\e[0m] No extra libraries found to copy."
    else
        echo -e "\n[\e[33m🚀\e[0m] Copying ${#copy_queue[@]} files to: $DEST_DIR"
        
        for file in "${copy_queue[@]}"; do
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
