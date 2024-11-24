#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <termios.h>
#include <time.h>

#define SAMPLE_COUNT 60
#define BAUD_RATE B9600 // Adjust for your arduino baud rate
#define DATA_FILE "data.csv"
#define DEVICE_FILE "/dev/ttyACM0" // Adjust for your arduino device file

int configureSerialPort(int fd);
float getAverage(float data[], int size);
void collectDataLoop(FILE* dev_file, FILE* data_file);    

// This program collects 60 samples per hour and writes the hourly average to a CSV file 
// named "data.csv". Sampling begins at the start of the next hour after the program is launched, 
// ensuring each hour's data is precisely aligned with the clock.
// Note: You may have to change the device file variable to the one your arduino writes to.
int main() {
    const char* device_file = DEVICE_FILE;
    int fd = open(device_file, O_RDONLY);
    if (fd == -1) {
        perror("Error opening device file");
        return 1;
    }

    if (configureSerialPort(fd) != 0) {
        close(fd);
        return 1;
    }

    FILE* dev_file = fdopen(fd, "r");
    if (dev_file == NULL) {
        perror("Error opening device file as file stream");
        fclose(dev_file);
        return 1;
    }

    // Checks if file already exists
    int dataFileExist = (access(DATA_FILE, F_OK) == 0);
    
    FILE* data_file = fopen(DATA_FILE, "a");
    if (data_file == NULL) {
        perror("Error opening data file");
        fclose(data_file);
        return 1;
    }
    
    // Print header if file didn't exist
    if (!dataFileExist) {
        fprintf(data_file, "StartUtc,PM2_5,PM10,NO2,Temp\n");
        fflush(data_file);
    } 
    
    collectDataLoop(dev_file, data_file);

    fclose(data_file);
    fclose(dev_file);
    return 0;
}

// Collect and log data from device
void collectDataLoop(FILE* dev_file, FILE* data_file) {
    float pm2_5_samples[SAMPLE_COUNT] = {0};
    float pm10_samples[SAMPLE_COUNT] = {0};
    float no2_samples[SAMPLE_COUNT] = {0};
    float temp_samples[SAMPLE_COUNT] = {0};
    char buffer[128];
    int count = 0;

    // Start time for every hour
    time_t startUTC = time(NULL);
    printf("Data collection starts in: %ld minutes\n", 60 - (startUTC % 3600) / 60);
    while (startUTC % 3600 != 0) {
        usleep(1000);  // Wait 1 milli-second, to avoid overclocking the cpu
        startUTC = time(NULL);
    }
    printf("Collecting data...\n");

    clock_t start, end;
    while (1) {
        start = clock();
        if (count == SAMPLE_COUNT) {
            fprintf(data_file, "%ld,%3.1f,%3.1f,%3.1f,%3.1f\n",
                (long)startUTC,
                getAverage(pm2_5_samples, SAMPLE_COUNT),
                getAverage(pm10_samples, SAMPLE_COUNT),
                getAverage(no2_samples, SAMPLE_COUNT),
                getAverage(temp_samples, SAMPLE_COUNT));
            fflush(data_file);  // Ensure data is immediately written to file
            startUTC = time(NULL);
            count = 0;  // Reset count for next batch
            printf("Collected data at unix time: %ld\n", startUTC);
        }

        if (fgets(buffer, sizeof(buffer), dev_file) != NULL) {
            float pm2_5, pm10, no2, temp;
            if (sscanf(buffer, "%f,%f,%f,%f", &pm2_5, &pm10, &no2, &temp) == 4) {
                pm2_5_samples[count] = pm2_5;
                pm10_samples[count] = pm10;
                no2_samples[count] = no2;
                temp_samples[count] = temp;
            } else { // Invalid samples are marked with a negative number
                pm2_5_samples[count] = -1;
                pm10_samples[count] = -1;
                no2_samples[count] = -1;
                temp_samples[count] = -1;
                fprintf(stderr, "Error parsing input: %s\n", buffer);
            }
            count++;

            end = clock();
            usleep(60000000 - (end - start));  // Wait 1 minute, minus the cpu time used
        } else {
            perror("Error reading from device file");
            break;
        }
    }
}

// Compute the average of samples, and throw out invalid samples
float getAverage(float data[], int size) {
    float sum = 0;
    int n = size;
    for (int i = 0; i < size; i++) {
        if (data[i] < 0) {
            n -= 1;
        } else {
            sum += data[i];
        }     
    }

    if (n < 1) 
        return 0;

    return sum / n;
}

// Function to set the serial port to blocking mode and configure settings
int configureSerialPort(int fd) {
    struct termios options;

    // Get current options for the port
    tcgetattr(fd, &options);

    // Set the baud rates to 9600
    cfsetispeed(&options, BAUD_RATE);
    cfsetospeed(&options, BAUD_RATE);

    // Set data bits, stop bits, and parity (8N1)
    options.c_cflag &= ~PARENB; // No parity
    options.c_cflag &= ~CSTOPB; // 1 stop bit
    options.c_cflag &= ~CSIZE;  // Mask the character size bits
    options.c_cflag |= CS8;     // 8 data bits

    // Enable the receiver and set local mode
    options.c_cflag |= (CLOCAL | CREAD);

    // Set blocking mode
    options.c_cc[VMIN] = 1;  // Minimum number of characters to read
    options.c_cc[VTIME] = 0; // Timeout (unused in blocking mode)

    // Apply the settings to the port
    if (tcsetattr(fd, TCSANOW, &options) != 0) {
        perror("Failed to configure serial port");
        return -1;
    }

    return 0;
}