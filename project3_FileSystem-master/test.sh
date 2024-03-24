#!/bin/bash

# the flies directory stores the stdout from each test.
# the swamps directory stores the dump after each test.

# To create hex data for tests run the following command
# python <<< 's="Hello World"; print "Size:", len(s), "\nHex Data:", "".join(map(lambda x:hex(ord(x))[2:].zfill(2), s)).upper()'
# Remember that every test should end with 'e'

SOURCEFILE="myfs.c"
DO_COMPILE=1
DO_CLEANUP=1
QUIT_ON_ERROR=0
SINGLE_TEST=0
TEST_FILE=0

usage()
{
cat <<TEXT
test.sh -- test your myfs
Options:
    -q    Quit on any error.
    -d    Skip clean up on exit.
    -h    Print this screen.
    -n X  Run test X without clean up.
TEXT

exit 0
}

clean_all()
{
    make clean-all &> /dev/null
    if [[ $? -ne 0 ]]; then
        echo "Error Cleaning! :("
        echo "Run 'make clean-all' to see error."
        exit 1
    fi
    rm -rf flies swamps
}

clean()
{
   make clean-all &> /dev/null
	if [[ $? -ne 0 ]]; then
    	echo "Error Cleaning! :("
    	echo "Run 'make clean-all' to see error."
    	exit 1
	fi
}

while getopts "qdhn:" OPTION
do
    case $OPTION in
        q)
            QUIT_ON_ERROR=1
            echo "Wlll quit on any error."
            ;;
        d)
            DO_CLEANUP=0
            echo "Will skip final cleanup."
            ;;
        n)
            TEST_NUM=${OPTARG}
            TEST_FILE=$(ls tests/${TEST_NUM}_*.test 2> /dev/null)
            if [[ $? -ne 0 ]]; then
                echo "Invalid test number: $TEST_NUM"
                echo
                usage
            fi
            n=$(echo "$TEST_FILE" | wc -l)
            if [[ n -ne 1 ]]
            then
                usage
            fi
            SINGLE_TEST=1
            ;;
        h)
            usage
            ;;
    esac
done

echo "Pre-Cleaning..."
clean_all
mkdir flies swamps

if [[ $DO_COMPILE == 1 ]]
then
    echo "Compiling..."
    make &> /dev/null
    if [[ $? -ne 0 ]]
    then
        echo "Error compiling (run 'make' to see error)."
        exit 1
    fi
fi

BANNED="strcpy strcat strtok sprintf vsprintf gets strlen strcmp"

echo "Checking for banned functions: $BANNED"
for bfunc in $BANNED
do
    if grep -E "\s${bfunc}\s*\(" $SOURCEFILE &> /dev/null
    then
        echo "BANNED FUNCTION '$bfunc' DETECTED on line:"
        echo
        grep -nE "\s${bfunc}\s*\(" $SOURCEFILE
        echo
        echo "Remove all banned functions from your code and try again."
        exit 1
    else
        echo "    $bfunc"
    fi
done
echo "All is good here."
echo

if [[ $SINGLE_TEST == 1 ]]
then

    echo "Will run test: $TEST_NUM"

    name=$(echo $(basename $TEST_FILE) | cut -d"." -f1)
    echo "Name: '$name'"
    ./fly_swamp fs.iso < "$TEST_FILE" > "flies/$name"
    if [[ $? -eq 0 ]]; then
        ./read_fs.py dump -p > "swamps/$name"
        if [[ $? -eq 0 ]]; then
            diff "flies/$name" "tests/good/flies/$name" &> /dev/null
            if [[ $? -ne 0 ]]; then
                echo "Test '$name' Failed :("
            else
                diff "swamps/$name" "tests/good/swamps/$name" &> /dev/null
                if [[ $? -ne 0 ]]; then
                    echo "Test '$name' Failed :("
                else
                    echo "Test '$name' Passed :)"
                fi
            fi
        else
            echo "Test '$name' Failed :("
        fi
    else
        echo "Test '$name' Failed :("
    fi
    exit 0
fi

tests=0
good=0

for t in $(ls tests/*.test | sort -n -t / -k 2)
do
#    echo "Cleaning and compiling..."
    make clean &> /dev/null; make &> /dev/null
    if [[ $? -ne 0 ]]; then
        echo "Error Cleaning or Compiling! :("
        echo "Run 'make clean; make' to see error."
        exit 1
    fi
    name=$(echo $(basename $t) | cut -d"." -f1)
#    echo "Running test '$name'..."
    ./fly_swamp fs.iso < "$t" > "flies/$name"
    if [[ $? -eq 0 ]]; then
        ./read_fs.py dump -p > "swamps/$name"
        if [[ $? -eq 0 ]]; then
            diff "flies/$name" "tests/good/flies/$name" &> /dev/null
            if [[ $? -ne 0 ]]; then
                echo "Test '$name' Failed :("
                if [[ $QUIT_ON_ERROR == 1 ]]; then
                    break
                fi
            else
                diff "swamps/$name" "tests/good/swamps/$name" &> /dev/null
                if [[ $? -ne 0 ]]; then
                    echo "Test '$name' Failed :("
                    if [[ $QUIT_ON_ERROR == 1 ]]; then
                        break
                    fi
                else
                    echo "Test '$name' Passed :)"
                    ((good++))
                fi
            fi
        else
            echo "Test '$name' Failed :("
            if [[ $QUIT_ON_ERROR == 1 ]]; then
                break
            fi
        fi
    else
        echo "Test '$name' Failed :("
        if [[ $QUIT_ON_ERROR == 1 ]]; then
            break
        fi
    fi
    ((tests++))

done

if [[ $DO_CLEANUP == 1 ]]
then
    echo "Post-Cleaning..."
    clean
fi

echo
echo "Passed $good tests of the $tests tested."
echo
echo "Thanks for using the Fly Swamp tester!"
echo "Please come again soon."

