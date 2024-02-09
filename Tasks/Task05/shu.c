/**
 * @file SerialHexUploader.c
 * @brief SerialHexUploader - A program to upload Intel HEX files via serial port.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <termios.h>
#include <string.h>
#include <sys/time.h>
#include <sys/select.h>

#define SERIAL_PORT "/dev/ttyUSB0"	/**< Serial port device path */
#define RESPONSE_TIMEOUT_SEC 1		/**< Timeout in seconds for response */

/**
 * @brief Main function to upload Intel HEX files via serial port.
 * @return Returns 0 on success, -1 on failure.
 */
int main(int argc, char *argv[])
{

    if (argc != 2) {
	printf("Usage: %s <HEX_File>\n", argv[0]);
	return -1;
    }

    int fd; /**< File descriptor for the serial port */
    struct termios serialPortSettings; /**< Structure to hold the settings for the serial port */

    /**< Open the serial port in blocking mode */
    fd = open(SERIAL_PORT, O_RDWR);
    if (fd == -1) {
	perror("Error opening serial port");
	exit(EXIT_FAILURE);
    }

    /**< Get the current serial port settings */
    tcgetattr(fd, &serialPortSettings);

    /**< Set the baud rate to 9600 */
    cfsetispeed(&serialPortSettings, B9600);
    cfsetospeed(&serialPortSettings, B9600);

    /**< Set other serial port settings */
    serialPortSettings.c_cflag &= ~PARENB;   /**< No parity */
    serialPortSettings.c_cflag &= ~CSTOPB;   /**< One stop bit */
    serialPortSettings.c_cflag &= ~CSIZE;    /**< Clear data size bits */
    serialPortSettings.c_cflag |= CS8;	     /**< 8 bits per byte */
    serialPortSettings.c_cflag &= ~CRTSCTS;  /**< No hardware flow control */
    serialPortSettings.c_cflag |= CREAD | CLOCAL; /**< Enable receiver, ignore control lines */

    /**< Apply the new settings */
    tcsetattr(fd, TCSANOW, &serialPortSettings);

    /**< Open the hex file */
    FILE *hexFile = fopen(argv[1], "r");
    if (hexFile == NULL) {
	perror("Error opening hex file");
	close(fd);
	exit(EXIT_FAILURE);
    }

    /**< Read lines from the hex file and send them over serial */
    char line[256]; /**< Buffer to store each line read from the hex file */
    size_t index = 0; /**< Index to keep track of the current position in the line buffer */
    int c; /**< Variable to store the character read from the hex file */
    int cLine; /**< Variable to store the character read from the hex file for each line */

    while ((c = fgetc(hexFile)) != EOF) {
	while ((cLine = fgetc(hexFile)) != '\n') {

            /**< Add character to the line buffer */
	    if (index < sizeof(line) - 1) { /**< Ensure there is space in the buffer */
		line[index++] = cLine;
	    } else {
	    /**< Line too long, we can handle error or truncate line */
		perror("Long line, more than expected");
	    }


	}
	if (cLine == '\n') {
	    /**< End of line reached, send the line over serial */
	    line[index + 1] = '\0';
	    /**< Null-terminate the string */
	    write(fd, line, strlen(line));
	    index = 0; /**< Reset index for the next line */
	}


	/**< Wait for response from the device with a timeout */
	fd_set rfds;
	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	struct timeval tv;
	tv.tv_sec = RESPONSE_TIMEOUT_SEC;
	tv.tv_usec = 0;
	int retval = select(fd + 1, &rfds, NULL, NULL, &tv);
	if (retval == -1) {
	    perror("Error in select");
	    exit(EXIT_FAILURE);
	} else if (retval == 0) {
	    fprintf(stderr, "Error: Timeout waiting for response\n");
	    break; /**< Break out of the loop and return error */
	} else {
	    /**< Response received, read and process it */
	    char response[3];
	    ssize_t bytesRead = read(fd, response, sizeof(response) - 1);
	    if (bytesRead > 0) {
		response[bytesRead] = '\0'; /**< Null-terminate the response string */
		if (strcmp(response, "ok") != 0) {
		    fprintf(stderr, "Error: Unexpected response '%s'\n",
			    response);
		}
	    } else {
		perror("Error reading from serial port");
	    }
	}

	/**< Clear the line buffer for the next iteration */
	memset(line, 0, sizeof(line));
    }

    /**< Notify user that flashing is done successfully */
    printf("Flash done successfully!\n");

    /**< Close the hex file */
    fclose(hexFile);

    /**< Close the serial port */
    close(fd);

    return 0;
}
