#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>

/*
 * calculate temperature[C] and humidity[%RH]
 */
static void calc_sth11(float *p_humidity, float *p_temperature)
{
	const float C1 = -2.0468;
	const float C2 = 0.0367;
	const float C3 = -0.0000015955;
	const float T1 = 0.01;
	const float T2 = 0.00008;
	
	float rh = *p_humidity;
	float t = *p_temperature;
	float rh_lin;
	float rh_true;
	float t_C;
	
	t_C = t*0.01 - 40.1;
	rh_lin = C3 * rh * rh + C2 * rh + C1;
	rh_true = (t_C - 25) * (T1 + T2 * rh) + rh_lin;
	if (rh_true > 100) rh_true = 100;
	if (rh_true < 0.1) rh_true = 0.1;
	
	*p_temperature = t_C;
	*p_humidity = rh_true;
}

int fd_jiffies = -1;
typedef int TICK;
TICK system_tick[3];

int main (int argc, char *argv[])
{
	int fd;
	unsigned char buf[4];
	int length;
	
	fd = open("/dev/sht11_dev", O_RDONLY);
	if (fd == -1) {
		printf("open failed!\n");
		return -1;
	}
	
	fd_jiffies = open("/dev/jiffies_dev", 0);
	if (fd_jiffies < 0) {
		printf("open jiffies failed! \r\n");
		return 0;
	}
	system_tick[0] = (TICK) ioctl(fd_jiffies, 0, 0);

	
	while (1) {
		system_tick[0] = (TICK) ioctl(fd_jiffies, 0, 0);
		length = read(fd, buf, 4);
		system_tick[1] = (TICK) ioctl(fd_jiffies, 0, 0);
		system_tick[2] = system_tick[1] >= system_tick[0] ? system_tick[1] - system_tick[0] : system_tick[1] + 0xffffffff - system_tick[0];
		if (length == -1) {
			printf("read error!\n");
			return -1;
		}
#if 0		
		int i;
		for (i = 0; i < 4; i++)
			printf("%0x  ", buf[i]);
		printf("\r\n");
#else		
		float humi, temp;
		humi = (float)(buf[0] + buf[1]*256);
		temp = (float)(buf[2] + buf[3]*256);
		calc_sth11(&humi, &temp);
		printf("HUMIDITY: %5.1f%%\r\n", humi);
		printf("TEMPERATURE: %5.1fC\r\n", temp);
		printf("Start read time point: %d ms\r\n End Read time point: %d ms\r\nREAD total time: %d ms\r\n", system_tick[0], system_tick[1], system_tick[2]);
#endif		
		sleep(2);
	}
}

