#!/usr/bin/env bash

# --- Configuration ---
# Manjaro stores default package lists in these files
DEFAULT_LISTS=("/rootfs-pkgs.txt" "/desktopfs-pkgs.txt")

if [ -z "$1" ]; then
    echo "Usage: $0 <path_to_executable>"
    exit 1
fi

TARGET="$1"

if [ ! -r "$TARGET" ]; then
    echo "Error: Cannot read target file '$TARGET'"
    exit 1
fi

echo "Loading system default definitions..."

# 1. Create an associative array (hash map) of original packages
#    We use awk '{print $1}' to strip version numbers from the text files.
declare -A original_pkgs

for list_file in "${DEFAULT_LISTS[@]}"; do
    if [ -f "$list_file" ]; then
        while read -r pkg_name rest_of_line; do
            # Store only the name as a key for fast lookup
            if [ -n "$pkg_name" ]; then
                original_pkgs["$pkg_name"]=1
            fi
        done < "$list_file"
    fi
done

echo "Analyzing dependencies for: $TARGET"
echo "===================================================================="

# Initialize lists
list_default=()
list_explicit=()
list_dependency=()
list_custom=()
list_missing=()

# 2. Process ldd output
while read -r line; do
    # Regex to capture the path: "libname.so => /path/to/lib (address)"
    if [[ "$line" =~ =\>\ ([^[:space:]]+) ]]; then
        lib_path="${BASH_REMATCH[1]}"
        
        # Get the owning package name purely (quiet mode)
        # Returns exit code 1 if file is not owned by any package
        if pkg_owner=$(pacman -Qqo "$lib_path" 2>/dev/null); then
            
            # CHECK: Is this package name in our "Original ISO" map?
            if [[ -n "${original_pkgs[$pkg_owner]}" ]]; then
                list_default+=( "$lib_path (Pkg: $pkg_owner)" )
                
            # If not in original map, check install reason
            else
                # -Qe = Explicitly installed, -Qd = Dependency
                # We check if it is explicit
                if pacman -Qqe "$pkg_owner" &>/dev/null; then
                    list_explicit+=( "$lib_path (Pkg: $pkg_owner)" )
                else
                    list_dependency+=( "$lib_path (Pkg: $pkg_owner)" )
                fi
            fi
        else
            # No owner found -> Custom
            list_custom+=( "$lib_path" )
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

echo "===================================================================="
