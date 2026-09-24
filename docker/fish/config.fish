set -g fish_greeting ""

function fish_prompt
    set_color brcyan
    echo -n "rubik"
    set_color normal
    echo -n " "
    set_color bryellow
    echo -n (basename (pwd))
    set_color normal
    echo -n " > "
end

alias ll "ls -lah --color=auto"
alias ls "ls --color=auto"

function m --description "make (mandatory binary)"
    make -C /workspace $argv
end

function mb --description "make bonus (raylib renderer)"
    make -C /workspace bonus $argv
end

function t --description "run the unit tests"
    make -C /workspace test
end

# valgrind with the strictness a 42 defense expects
function vg --description "valgrind, full strictness"
    valgrind --leak-check=full --show-leak-kinds=all \
             --track-origins=yes --track-fds=yes \
             --errors-for-leak-kinds=all --error-exitcode=42 $argv
end

function gl --description "which OpenGL is the display giving us"
    glxinfo -B 2>&1 | grep -E 'renderer|core profile version|direct rendering|unable|Error'
end

set -l gui "no DISPLAY (mandatory part only)"
if set -q DISPLAY
    set gui "DISPLAY=$DISPLAY"
end
echo "rubik dev box — "(uname -m)"  |  gcc "(gcc -dumpversion)"  valgrind "(valgrind --version | cut -d- -f2)
echo "gui: $gui"
echo "helpers: m (make) · mb (make bonus) · t (tests) · vg CMD (valgrind) · gl (GL info) · check-env"
echo ""
