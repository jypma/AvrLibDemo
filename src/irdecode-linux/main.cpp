#include <iostream>
#include <fcntl.h>
#include <linux/input.h>
#include <linux/uinput.h>
#include <string.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <regex>
#include <errno.h>      // Error number definitions
#include <termios.h>    // POSIX terminal control definitions
#include <map>

int read_line(const int fd, char *target, int maxlength) {
    *target = 0;
    int count = 0;
    while (maxlength > 0) {
        int result;
        if ((result = read(fd, target, 1)) == 1) {
            count++;
            if (*target == '\n') {
                *target = 0;
                return count;
            } else {
                target++;
                maxlength--;
            }
        } else {
            return result;
        }
    }
    return -1;
}

void setTerm(const int USB) {
    struct termios tty;
    struct termios tty_old;
    memset (&tty, 0, sizeof tty);

    /* Error Handling */
    if ( tcgetattr ( USB, &tty ) != 0 ) {
       std::cout << "Error " << errno << " from tcgetattr: " << strerror(errno) << std::endl;
    }

    /* Save old tty parameters */
    tty_old = tty;

    /* Set Baud Rate */
    cfsetospeed (&tty, (speed_t)B57600);
    cfsetispeed (&tty, (speed_t)B57600);

    /* Setting other Port Stuff */
    tty.c_cflag     &=  ~PARENB;            // Make 8n1
    tty.c_cflag     &=  ~CSTOPB;
    tty.c_cflag     &=  ~CSIZE;
    tty.c_cflag     |=  CS8;

    tty.c_cflag     &=  ~CRTSCTS;           // no flow control
    tty.c_cc[VMIN]   =  1;                  // read doesn't block
    tty.c_cc[VTIME]  =  5;                  // 0.5 seconds read timeout
    tty.c_cflag     |=  CREAD | CLOCAL;     // turn on READ & ignore ctrl lines

    /* Make raw */
    cfmakeraw(&tty);

    /* Flush Port, then applies attributes */
    tcflush( USB, TCIFLUSH );
    if ( tcsetattr ( USB, TCSANOW, &tty ) != 0) {
       std::cout << "Error " << errno << " from tcsetattr" << std::endl;
    }
}

void pressKey(const int fd, const int key) {
    struct input_event ev;
    memset(&ev, 0, sizeof(ev));

    ev.type = EV_KEY;
    ev.code = key;
    ev.value = 1;
    gettimeofday(&ev.time, NULL);

    if (write(fd, &ev, sizeof(ev)) == -1) {
        std::cerr << "Could not send a key event." << std::endl;
        exit(-1);
    }

    usleep(50000);

    ev.type = EV_KEY;
    ev.code = key;
    ev.value = 0;
    gettimeofday(&ev.time, NULL);

    if (write(fd, &ev, sizeof(ev)) == -1) {
        std::cerr << "Could not send a key event." << std::endl;
        exit(-1);
    }

    ev.type = EV_SYN;
    ev.code = SYN_REPORT;
    ev.value = 0;
    if(write(fd, &ev, sizeof(struct input_event)) < 0) {
        std::cerr << "Could not sync." << std::endl;
        exit(-1);
    }
}

int main() {

    // This is for Yamaha DVD, 2064
    std::map<int,int> ircode_to_keycode;
    ircode_to_keycode[1052847570] = KEY_UP;
    ircode_to_keycode[1052888370] = KEY_DOWN;
    ircode_to_keycode[1052880210] = KEY_LEFT;
    ircode_to_keycode[1052863890] = KEY_RIGHT;
    ircode_to_keycode[1052843490] = KEY_ENTER;
    ircode_to_keycode[1052896530] = KEY_BACKSPACE;
    ircode_to_keycode[1052861850] = KEY_I;
    ircode_to_keycode[1052872050] = KEY_ESC;
    ircode_to_keycode[1052898060] = KEY_C;
    ircode_to_keycode[1052899080] = KEY_MODE;
    ircode_to_keycode[1052877150] = KEY_X;
    ircode_to_keycode[1052885310] = KEY_SPACE;
    ircode_to_keycode[1052852670] = KEY_PLAY;
    ircode_to_keycode[1052860830] = KEY_REWIND;
    ircode_to_keycode[1052893470] = KEY_FASTFORWARD;
    ircode_to_keycode[1052876130] = KEY_PREVIOUS;
    ircode_to_keycode[1052859810] = KEY_NEXT;
    ircode_to_keycode[1052846550] = KEY_1;
    ircode_to_keycode[1052879190] = KEY_2;
    ircode_to_keycode[1052862870] = KEY_3;
    ircode_to_keycode[1052895510] = KEY_4;
    ircode_to_keycode[1052842470] = KEY_5;
    ircode_to_keycode[1052875110] = KEY_6;
    ircode_to_keycode[1052858790] = KEY_7;
    ircode_to_keycode[1052891430] = KEY_8;
    ircode_to_keycode[1052850630] = KEY_9;
    ircode_to_keycode[1052887350] = KEY_0;
    ircode_to_keycode[1052883270] = KEY_102ND;
    ircode_to_keycode[1052866950] = KEY_EDIT;
    ircode_to_keycode[-25109980] = KEY_HOME;
    ircode_to_keycode[-25134460] = KEY_END;
    ircode_to_keycode[-25131400] = KEY_PAGEUP;
    ircode_to_keycode[-25155880] = KEY_PAGEDOWN;
    ircode_to_keycode[-25158940] = KEY_F1;
    ircode_to_keycode[-25122220] = KEY_F2;
    ircode_to_keycode[-25106920] = KEY_F3;

    int fd;

    fd = open("/dev/input/uinput", O_WRONLY | O_NONBLOCK);
    if(fd < 0) {
        fd = open("/dev/uinput", O_WRONLY | O_NONBLOCK);
        if (fd < 0) {
            std::cerr << "Could not open /dev/input/uinput or /dev/uinputfd" << std::endl;
            return -1;
        }
    }

    if (ioctl(fd, UI_SET_EVBIT, EV_KEY) == -1) {
        std::cerr << "Could not enable keyboard events." << std::endl;
        return -1;
    }

    ioctl(fd, UI_SET_EVBIT, EV_SYN);

    if (ioctl(fd, UI_SET_EVBIT, EV_REL) == -1) {
        std::cerr << "Could not enable keyboard events." << std::endl;
        return -1;
    }

    int i = 0;
    for (i = 0; i < 256; i++) {
        ioctl(fd, UI_SET_KEYBIT, i);
    }

    uinput_user_dev uidev;
    memset(&uidev, 0, sizeof(uidev));

    snprintf(uidev.name, UINPUT_MAX_NAME_SIZE, "uinput-sample");
    uidev.id.bustype = BUS_USB;
    uidev.id.vendor  = 0x1234;
    uidev.id.product = 0xfedc;
    uidev.id.version = 1;

    if (write(fd, &uidev, sizeof(uidev)) == -1) {
        std::cerr << "Could not write USB device." << std::endl;
        return -1;
    }

    if(ioctl(fd, UI_DEV_CREATE) != 0){
        std::cerr << "Could not register as USB device." << std::endl;
        return -1;
    }

    const char *input_fname = getenv("SERIAL_INPUT");
    if (input_fname == NULL) {
        std::cerr << "You must set SERIAL_INPUT to the input serial device to use, e.g. /dev/ttyUSB0." << std::endl;
        return -1;
    }

    errno = 0;
    int input = open(input_fname, O_RDWR | O_NOCTTY);
    if (!input) {
        std::cerr << "could not open for reading : " << input_fname << std::endl;
        return -1;
    }
    if (errno) {
        std::cout << "could not open for reading : " << input_fname << ", errno:  " << errno << std::endl;
        return -1;
    }
    setTerm(input);

    char line[256];
    int lastKey = -1;
    timeval lastKeyTime;

    while (read_line(input, line, 256) > 0) {
        std::string s(line);
        std::regex expected(".*([0-9]+),([0-9]+).*");
        std::smatch result;
        if (std::regex_search(s, result, expected)) {
            int type = atoi(result[1].str().c_str());
            int cmd = atoi(result[2].str().c_str());
            if (type == 0) {
                std::cout << cmd << std::endl;

                auto found = ircode_to_keycode.find(cmd);
                if(found != ircode_to_keycode.end()) {
                    int key = found->second;
                    std::cout << " pressing key " << key << std::endl;
                    lastKey = key;
                    gettimeofday(&lastKeyTime, nullptr);
                    pressKey(fd, key);
                } else {
                    lastKey = -1;
                }
            } else if (type == 1) {
                std::cout << "  *repeated*" << std::endl;
                timeval now;
                gettimeofday(&now, nullptr);
                if (lastKey != -1) {
                    auto diff = (now.tv_sec - lastKeyTime.tv_sec) * 1000 + ((now.tv_usec - lastKeyTime.tv_usec) / 1000);
                    if (diff > 200) {
                        pressKey(fd, lastKey);
                    }
                }
            } else {
                lastKey = -1;
            }
        }
    }

    std::cout << "errno:  " << errno << std::endl;

    if (ioctl(fd, UI_DEV_DESTROY) == -1) {
        std::cerr << "Warning: could not destroy event device." << std::endl;
        return -1;
    }
}
