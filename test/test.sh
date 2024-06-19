#!/bin/bash

set -e

RED='\033[1;31m'
GREEN='\033[1;32m'
LIGHT_PURPLE='\033[1;35m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

TEST_FAILED=0
TEST_FAILED_MSG=""

XFWM4_THEME=Basix # Border size top=24px bottom=5px left=5px right=5px
XFWM4_THEME_BAK=$(xfconf-query -c xfwm4 -p /general/theme)

assertEquals()
{
    expected=$1
    actual=$2

    if [ "$TEST_FAILED" -ne "0" ]; then
        return;
    fi

    if [ "${expected}" != "${actual}" ]; then
        TEST_FAILED_MSG="${actual} is not ${expected}"
        TEST_FAILED=1;
    fi
}

openTestWindow()
{
    /tmp/xfce4-screenshooter-test "$@" & PID=$!
    sleep 1
}

closeTestWindow()
{
    kill "$PID"
}

test()
{
    TEST_FAILED=0
    TEST_FAILED_MSG=""
    echo -e "${LIGHT_PURPLE}[TEST] ${PURPLE}$1${NC}"

    $1

    if [ "$TEST_FAILED" -eq "0" ]; then
        echo -e "${GREEN}OK${NC}"
    else
        echo -e "${RED}FAILED: ${TEST_FAILED_MSG}${NC}"
    fi

    echo
}

setUp()
{
    if [ ! -e /tmp/xfce4-screenshooter-test ]; then
        gcc $(pkg-config --cflags gtk+-3.0) test.c -o /tmp/xfce4-screenshooter-test $(pkg-config --libs gtk+-3.0)
    fi
    xfconf-query -c xfwm4 -p /general/theme -s $XFWM4_THEME
    
}

tearDown()
{
    xfconf-query -c xfwm4 -p /general/theme -s $XFWM4_THEME_BAK
}

testScreenshotActiveWindow()
{
    openTestWindow
    ../src/xfce4-screenshooter -w -s /tmp/test.png
    assertEquals '300x300' $(identify -format '%wx%h' /tmp/test.png)
    assertEquals 0 $(magick /tmp/test.png -format %c histogram:info:- | grep -v '#00FF00' | wc -l)
    closeTestWindow
}

testScreenshotActiveWindowDecoration()
{
    openTestWindow --decoration
    ../src/xfce4-screenshooter -w -s /tmp/test.png
    assertEquals '310x329' $(identify -format '%wx%h' /tmp/test.png)
    assertEquals 0 $(magick /tmp/test.png -crop 300x300+5+24 -format %c histogram:info:- | grep -v '#00FF00' | wc -l)
    closeTestWindow
}

testScreenshotActiveWindowDecorationScale2x()
{
    GDK_SCALE=2 openTestWindow --decoration
    ../src/xfce4-screenshooter -w -s /tmp/test.png
    assertEquals '610x629' $(identify -format '%wx%h' /tmp/test.png)
    assertEquals 0 $(magick /tmp/test.png -crop 300x300+5+24 -format %c histogram:info:- | grep -v '#00FF00' | wc -l)
    closeTestWindow
}

testScreenshotRegion()
{
    openTestWindow
    ../src/xfce4-screenshooter -r -s /tmp/test.png &
    sleep 1
    xdotool mousemove 100 100
    xdotool mousedown 1
    sleep 1
    xdotool mousemove 399 399 # 300x300, inclusive
    xdotool mouseup 1
    sleep 1
    assertEquals '300x300' $(identify -format '%wx%h' /tmp/test.png)
    assertEquals 0 $(magick /tmp/test.png -format %c histogram:info:- | grep -v '#00FF00' | wc -l)
    closeTestWindow
}

testScreenshotFullscreen()
{
    ../src/xfce4-screenshooter -f -s /tmp/test.png
    assertEquals $(xrandr --current | grep '*' | awk '{print $1}') $(identify -format '%wx%h' /tmp/test.png)
}

setUp
test testScreenshotActiveWindow
test testScreenshotActiveWindowDecoration
test testScreenshotActiveWindowDecorationScale2x
test testScreenshotRegion
test testScreenshotFullscreen
tearDown
