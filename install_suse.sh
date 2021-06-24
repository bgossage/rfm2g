#!/bin/bash

function print_version ()
{
    echo "
Version 1.0 2020-08-06 by Paulus Janssen, NTA

This bash script copies the driver, API and library to the proper system folders.
After this installation the driver will be loaded automatically during system
startup, and the API can be used from its location in /usr/local/.

The script must be executed as superuser (sudo) from the project folder. "
}

function print_help ()
{
    echo "
Usage: $MESCRIPT [OPTIONS]

Options:
  -h,  --help           help information
  -v,  --version        version information"
}

function exit_on_error ()
{
    if [ $1 -ne 0 ]; then exit 1; fi
}

function check_exist ()
{
    ERRS=0
    for x in $1; do
        FOUND=0
        for dir in ${PATH//:/ }; do
            if [ -x $dir/$x ]; then
                FOUND=1
                break;
            fi  
        done
        if [ $FOUND = 0 ]; then
            echo "$MESCRIPT: Cannot locate executable: $x"
            ERRS=1
        fi  
    done
    return $ERRS
}

PATH=/usr/bin:/bin:/sbin
MESCRIPT=$(basename "$0")
ERR=0

# bash-builtins - bash built-in commands, see bash(1):
# -------------------------------------------------------------------------------------------------------------
# alias, bg, bind, break, builtin, case, cd, command, compgen, complete, continue, declare, dirs, disown,
# echo, enable, eval, exec, exit, export, fc, fg, getopts, hash, help, history, if, jobs, kill, let, local,
# logout, popd, printf, pushd, pwd, read, readonly, return, set, shift, shopt, source, suspend, test, times,
# trap, type, typeset, ulimit, umask, unalias, unset, until, wait, while
# -------------------------------------------------------------------------------------------------------------

# Verify that certain utility programs in $PATH do exist
check_exist "getopt"
exit_on_error $?

SHRT_OPT="hv"
LONG_OPT="help,version"
TEMP=$(getopt -o $SHRT_OPT --long $LONG_OPT -n "$MESCRIPT" -- "$@")

# Check the getopt return value
if [ $? -ne 0 ]; then
    print_help
    exit 1
fi

# The following variables are set by options
OPTS=":"
DO_EXIT=0

# Process the options
eval set -- "$TEMP"
while true
do
    case "$1" in
        -h|--help)      OPTS+="help:";    shift 1 ;;
        -v|--version)   OPTS+="version:"; shift 1 ;;

        --) shift 1; break ;;
        *) echo "$MESCRIPT: Internal error!"; exit 1 ;;
    esac
done

# Show the version menu
if [[ $OPTS =~ ":version:" ]]; then
    print_version
    DO_EXIT=1
fi

# Show the help menu
if [[ $OPTS =~ ":help:" ]]; then
    print_help
    DO_EXIT=1
fi

if [ $DO_EXIT -eq 1 ]; then exit 0; fi

# Check the script is run as superuser (sudo)
if [ $(id -u) -ne 0 ]; then
    echo "Must be superuser"
    exit 1
fi

# The following operations will be performed:
#
# 1. Verify that all files exist (also thos created via "make").
#    Unload, start/stop the driver if currently installed
# ---------------------------------------------------------------
# -------- To enable automatic load at startup
# ---------------------------------------------------------------
# 2. Copy "rfg2m.ko" to /lib/module/`uname -r`/misc/
#    owner root:root, mode 0644
# 3. Copy "rfm2g_init_ubuntu.service" to /lib/systemd/system/
#    owner root:root, mode 0644
# 4. Copy "rfm2g_init_ubuntu" to /etc/init.d/
#    owner root:root, mode 0755
# 5. Execute: systemctl daemon-reload
#    Execute: systemctl enable rfm2g_init_ubuntu.service --now
# ---------------------------------------------------------------
# -------- To install the API and library
# ---------------------------------------------------------------
# 6. Copy all ./include/*.h files to "/usr/local/include/rfm2g/"
#    owner root:root, mode 0644
# 7. Copy "librfm2g.a" to "/usr/local/lib/"
#    owner root:root, mode 0644
# 8. Execute: ldconfig

DEVICE=rfm2g
MOD_FILE=${DEVICE}.ko
LIB_FILE=lib${DEVICE}.a
START_STOP_FILE=${DEVICE}_init_suse
SERVICE_FILE=${DEVICE}_init_suse.service

MOD_DIR=/lib/modules/`uname -r`/extra
START_STOP_DIR=/etc/init.d
SERVICE_DIR=/etc/systemd/system
LIB_DIR=/usr/local/lib
INC_DIR=/usr/local/include

read -p "Are you sure you want to install the $DEVICE driver, library and API? (yes/no) " ANSW
if [ "${ANSW^^}" != "YES" ]; then
    echo "$MESCRIPT: Aborted"
    exit 0;
fi
echo

# Make sure DEVICE has a value
if [ -z "${DEVICE}" ]; then
    echo "$MESCRIPT: String DEVICE must be defined"
    exit 1
fi

# Make sure these files exist
for x in ./driver/$MOD_FILE ./api/$LIB_FILE ./$START_STOP_FILE ./$SERVICE_FILE; do
    if [ ! -f "$x" ]; then
        echo "$MESCRIPT: File not found: $x"
        ERR=1
    fi
done
if [ $ERR = 1 ]; then exit 1; fi

# Verify that additional utility programs in $PATH do exist
check_exist "chmod chown cp ls mkdir rm ldconfig systemctl"
exit_on_error $?

# Make sure these folders exist
for x in $MOD_DIR $START_STOP_DIR $SERVICE_DIR $LIB_DIR $INC_DIR; do
    if [ ! -d "$x" ]; then
        echo "$MESCRIPT: Folder not found: $x"
        ERR=1
    fi
done
if [ $ERR = 1 ]; then exit 1; fi

LIST=""

# Kernel module rfm2g.ko
cp ./driver/$MOD_FILE $MOD_DIR/
exit_on_error $?
FILE=$MOD_DIR/$MOD_FILE
chown root:root $FILE
chmod 0644 $FILE
LIST="$LIST $FILE"

# Service file rfm2g_init_ubuntu.service
cp ./$SERVICE_FILE $SERVICE_DIR/
exit_on_error $?
FILE=$SERVICE_DIR/$SERVICE_FILE
chown root:root $FILE
chmod 0644 $FILE
LIST="$LIST $FILE"

# Start stop script rfm2g_init_ubuntu
cp ./$START_STOP_FILE $START_STOP_DIR/
exit_on_error $?
FILE=$START_STOP_DIR/$START_STOP_FILE
chown root:root $FILE
chmod 0755 $FILE
LIST="$LIST $FILE"

# Library file librfm2g.a
cp ./api/$LIB_FILE $LIB_DIR/
exit_on_error $?
FILE=$LIB_DIR/$LIB_FILE
chown root:root $FILE
chmod 0644 $FILE
LIST="$LIST $FILE"

ls -lS --color=auto $LIST

# Include folder for API header files
FOLDER=$INC_DIR/$DEVICE
FILES=${DEVICE}_*.h
rm -f $FOLDER/$FILES
mkdir -p $FOLDER
chown root:root $FOLDER
chmod 0755 $FOLDER
echo
ls -ld --color=auto $FOLDER
# The API .h header files
cp ./include/$FILES $FOLDER/
exit_on_error $?
chown root:root $FOLDER/*.h
chmod 0644 $FOLDER/*.h
#echo 
#echo "$FOLDER/"
ls -l --color=auto $FOLDER

# Start the service, rescan /usr/local/lib
echo
echo "Start ${DEVICE} driver"
ldconfig
exit_on_error $?
systemctl daemon-reload
exit_on_error $?
systemctl enable rfm2g_init_suse.service --now
exit_on_error $?
echo
echo "Done."
