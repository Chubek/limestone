#!/usr/bin/env bash
# ============================================================================
# Simdette Format Checker
# ============================================================================
# Checks code formatting consistency
# ============================================================================

set -euo pipefail

RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

log_info() { echo -e "${GREEN}[INFO]${NC} $1"; }
log_warn() { echo -e "${YELLOW}[WARN]${NC} $1"; }
log_error() { echo -e "${RED}[ERROR]${NC} $1"; }

# Check for clang-format
if ! command -v clang-format &> /dev/null; then
    log_warn "clang-format not found. Install with: sudo apt install clang-format"
    exit 1
fi

# Check file types
check_files() {
    local files
    files=$(find include examples tests -name "*.hpp" -o -name "*.cpp" 2>/dev/null)
    
    if [ -z "$files" ]; then
        log_error "No C++ files found!"
        exit 1
    fi
    
    echo "$files"
}

# Check formatting
check_format() {
    local file
    local errors=0
    
    for file in $(check_files); do
        if ! clang-format --dry-run --Werror "$file" > /dev/null 2>&1; then
            log_error "Format error in: $file"
            errors=$((errors + 1))
        fi
    done
    
    return $errors
}

# Fix formatting
fix_format() {
    log_info "Fixing formatting..."
    
    for file in $(check_files); do
        clang-format -i "$file"
        log_info "Fixed: $file"
    done
}

# Check line lengths
check_line_lengths() {
    local max_length=100
    local file
    local line_num=0
    local errors=0
    
    for file in $(check_files); do
        line_num=0
        while IFS= read -r line; do
            line_num=$((line_num + 1))
            if [ ${#line} -gt $max_length ]; then
                log_warn "Line ${line_num} too long in $file: ${#line} chars"
                errors=$((errors + 1))
            fi
        done < "$file"
    done
    
    if [ $errors -eq 0 ]; then
        log_info "All lines within ${max_length} char limit"
    else
        log_warn "$errors lines exceed ${max_length} characters"
    fi
}

# Check for tabs
check_tabs() {
    local file
    local errors=0
    
    for file in $(check_files); do
        if grep -q $'\t' "$file"; then
            log_error "Tabs found in: $file"
            errors=$((errors + 1))
        fi
    done
    
    if [ $errors -eq 0 ]; then
        log_info "No tabs found (using spaces)"
    fi
}

# Check for trailing whitespace
check_trailing_whitespace() {
    local file
    local errors=0
    
    for file in $(check_files); do
        if grep -q '[[:space:]]$' "$file"; then
            log_warn "Trailing whitespace in: $file"
            errors=$((errors + 1))
        fi
    done
    
    if [ $errors -eq 0 ]; then
        log_info "No trailing whitespace"
    fi
}

# Check newlines
check_newlines() {
    local file
    local errors=0
    
    for file in $(check_files); do
        # Check if file ends with newline
        if [ -s "$file" ] && [ "$(tail -c 1 "$file" | wc -l)" -eq 0 ]; then
            log_warn "File $file does not end with newline"
            errors=$((errors + 1))
        fi
    done
    
    if [ $errors -eq 0 ]; then
        log_info "All files end with newline"
    fi
}

# Main
main() {
    local action="${1:-check}"
    
    case "$action" in
        check)
            log_info "Checking code format..."
            
            check_format || true
            check_line_lengths
            check_tabs
            check_trailing_whitespace
            check_newlines
            
            log_info "Format check complete"
            ;;
        
        fix)
            fix_format
            log_info "Formatting fixed"
            ;;
        
        diff)
            log_info "Showing format differences..."
            for file in $(check_files); do
                clang-format "$file" | diff -u "$file" - || true
            done
            ;;
        
        *)
            echo "Usage: $0 {check|fix|diff}"
            echo ""
            echo "Commands:"
            echo "  check - Check code formatting (default)"
            echo "  fix   - Fix formatting issues"
            echo "  diff  - Show format differences"
            exit 1
            ;;
    esac
}

main "$@"
