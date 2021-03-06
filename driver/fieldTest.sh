#!/usr/bin/env bash
# Start with `bash ./foreTest.sh`

if [[ "$(whoami)" != "pi" ]]; then
   echo "This script must be run as \"pi\""
   exit 1
fi

if [ "$(pwd)" != "/home/pi/VanderbiltRocketTeam" ]; then
   echo "This script must be run from /home/pi/VanderbiltRocketTeam" 
   exit 1
fi

if [ -z "$IN_NIX_SHELL" ]; then
    echo "THis script must be run within a nix-shell"
    exit 1
fi

hostname=$(hostname)
if [[ "$hostname" =~ ^sift.* ]]; then
    mode=sift
    echo "This is a SIFT pi (hostname starts with \"sift\": $hostname)"
else
    mode=video_cap
    echo "This is a video capture pi (hostname doesn't start with \"sift\": $hostname)"
fi

ctrl_c() {
    sudo pkill -SIGINT sift
    sudo pkill -SIGINT pigpio
}

exe=sift_exe_release_commandLine
trap ctrl_c SIGINT

echo "Running first camera test"

if [ "$mode" == "sift" ]; then
    crop=
    #crop=--crop-for-fisheye-camera
    ./$exe --extra-sift-exe-args "$crop --no-preview-window --finish-rest-always --fps $fps --save-first-image" --sift-params '-C_edge 2 -delta_min 0.6' $commonArgs $extraArgs 2>&1 | tee "./dataOutput/$(date +"%Y_%m_%d_%I_%M_%S_%p").$mode""log.txt" #| tee <(python3 "driver/radio.py" 1)
else
    sudo pigpiod
    python3 ./driver/cameraSwap.py 0
    ./$exe "--camera-test-only"
    echo "Running second camera test"
    sudo pigpiod
    python3 ./driver/cameraSwap.py 1
    ./$exe "--camera-test-only"
fi

echo "Running radio test"
python3 ./driver/radio.py


