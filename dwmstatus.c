#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <string.h>
#include <X11/Xlib.h>
#include <sys/utsname.h>
#include <sys/sysinfo.h>
#include <inttypes.h>

#include <alsa/asoundlib.h>
#include <alsa/mixer.h>

#define MB 1048576
#define GB 1073741824

#define OBJECTS 10
#define LENGTH 128
#define WHOLE_LENGTH LENGTH*OBJECTS

#define NETWORK_INTERFACE "wlp1s0"
#define TEMP_SENSORS 3

extern FILE *popen();
extern int pclose();

void set_status(const char *value) {
	Display *display = XOpenDisplay(NULL);
	
	if(display) {
		XStoreName(display, DefaultRootWindow(display), value);
		XSync(display, False);
		XCloseDisplay(display);
	} else {
		exit(EXIT_FAILURE);
	}
}

void get_date_time(char *value) {
	char time_str[LENGTH];
	time_t t = time(NULL);

	strftime(time_str, LENGTH, "%a %d.%m, %H:%M", localtime(&t));
	
	snprintf(value, LENGTH, "%s", time_str);
}

void get_cpu_temp(int *value) {
	FILE *fd;
	char temp_cmd[LENGTH];
	int temp;

	for(int i = 1; i <= TEMP_SENSORS; i++) {
		snprintf(temp_cmd, LENGTH, "/sys/bus/platform/devices/coretemp.0/hwmon/hwmon1/temp%d_input", i);
		fd = fopen(temp_cmd, "r");
		if(fd == NULL) {
			printf ("__func__ = %s :: Error opening file\n", __func__);
			value[i-1] = -1;
		}
		fscanf(fd, "%d", &temp);
		value[i-1] = temp / 1000;
	}

	fclose(fd);
}

void get_bat_capacity(int *value) {
	FILE *fd;

	fd = fopen("/sys/class/power_supply/BAT0/capacity", "r");
	if(fd == NULL) {
		printf ("__func__ = %s :: Error opening file\n", __func__);
		*value = -1;
		return;
	}
	fscanf(fd, "%d", value);
	fclose(fd);
}

void get_bat_status(char *value) {
	FILE *fd;

	fd = fopen("/sys/class/power_supply/BAT0/status", "r");
	if(fd == NULL) {
		printf ("__func__ = %s :: Error opening file\n", __func__);
	 	*value = -1;
	 	return;
	}
	fscanf(fd, "%s", value);
	fclose(fd);
}

void get_bat_time(char *value) {
	FILE *input;
	char output[LENGTH];

	char bat_status[LENGTH];
	get_bat_status(bat_status);

	if(strcmp(bat_status, "Discharging") == 0 || strcmp(bat_status, "Charging") == 0) {
		if(!(input = popen("acpi -b -d /sys/class/ | cut -d ' ' -f 5", "r"))) {
			printf ("__func__ = %s :: Error executing command\n", __func__);
			snprintf(value, LENGTH, "Error");
			return;
		}
		if(fgets(output, sizeof(output), input) != NULL) {
			if(output[strlen(output) - 1] == '\n') {
				output[strlen(output) - 1] = '\0';
			}
			snprintf(value, LENGTH, "%s", output);
		}	
		pclose(input);
	} else {
		snprintf(value, LENGTH, "Full");
	}
}

void get_brightness(int *value) {
	FILE *fd;

	fd = fopen("/sys/class/backlight/acpi_video0/brightness", "r");
	if(fd == NULL) {
		printf ("__func__ = %s :: Error opening file\n", __func__);
		*value = -1;
		return;
	}
	fscanf(fd, "%d", value);
	fclose(fd);
}

void get_kernel(char *value) {
	struct utsname kernel;

	uname(&kernel);
	snprintf(value, LENGTH, "%s %s", kernel.sysname, kernel.release);
}

/**
 * @todo: get capture info
 */
void get_volume(char *value) {	
    long int vol, max, min, percent;
    int mute;
	
	snd_mixer_t *handle;
    snd_mixer_elem_t *elem;
    snd_mixer_selem_id_t *s_elem;

    snd_mixer_open(&handle, 0);
    snd_mixer_attach(handle, "default");
    snd_mixer_selem_register(handle, NULL, NULL);
    snd_mixer_load(handle);
    snd_mixer_selem_id_malloc(&s_elem);
    snd_mixer_selem_id_set_name(s_elem, "Master");

    elem = snd_mixer_find_selem(handle, s_elem);

    if(elem == NULL) {
		printf ("__func__ = %s :: Error getting status\n", __func__);
		snd_mixer_selem_id_free(s_elem);
		snd_mixer_close(handle);
	}
	
	// mirco
	// snd_mixer_selem_get_capture_switch
	snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_UNKNOWN, &mute);
	if(mute == 1) {
		snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
		snd_mixer_selem_get_playback_volume(elem, 0, &vol);

		percent = (vol * 100) / max;
		snprintf(value, LENGTH, "%ld%%", percent);
	} else {
		*value = -1;
		snprintf(value, LENGTH, "mute");
	}
    
    snd_mixer_selem_id_free(s_elem);
    snd_mixer_close(handle);
}

void get_cpu_usage(int *value) {
	FILE *input;
	char output[LENGTH];

	// sysstat required
	if(!(input = popen("mpstat | awk '$12 ~ /[0-9.]+/ {print 100 - $12}'", "r"))) {
		printf ("__func__ = %s :: Error executing command\n", __func__);
		*value = -1;
		return;
	}

	if(fgets(output, sizeof(output), input) != NULL) {
		if(output[strlen(output) - 1] == '\n') {
			output[strlen(output) - 1] = '\0';
		}
		*value = atoi(output);
	}

	pclose(input);
}

void get_ram_usage(int *value) {
	uintmax_t used = 0, total = 0, percent = 0; 
    struct sysinfo mem;

    sysinfo(&mem);
    total = (uintmax_t) mem.totalram / MB;
    used = (uintmax_t) (mem.totalram - mem.freeram) / MB;

    percent = (used * 100) / total;
	*value = percent;
}

void get_network_status(char *value) {
	FILE *fd;
	char network_cmd[LENGTH];

	snprintf(network_cmd, LENGTH, "/sys/class/net/%s/operstate", NETWORK_INTERFACE);

	fd = fopen(network_cmd, "r");
	if(fd == NULL) {
		printf ("__func__ = %s :: Error opening file\n", __func__);
		snprintf(value, LENGTH, "Error");
	}
	fscanf(fd, "%s", value);
	fclose(fd);
}

int main(void) {
	char status[WHOLE_LENGTH];
	char datetime[LENGTH];
	// char str_bat_status[LENGTH];
	char bat_time[LENGTH];
	char kernel[LENGTH];
	char str_cpu_temp[LENGTH];
	char volume[LENGTH];
	char network_status[LENGTH];

	int bat_capacity, cpu_usage, ram_usage, brightness;
	int cpu_temp[TEMP_SENSORS];

	get_date_time(datetime);
	get_cpu_temp(cpu_temp);
	get_cpu_usage(&cpu_usage);
	get_bat_capacity(&bat_capacity);
	// get_bat_status(bat_status);
	get_bat_time(bat_time);
	get_brightness(&brightness);
	get_volume(volume);
	get_kernel(kernel);
	get_ram_usage(&ram_usage);
	get_network_status(network_status);

	// create temp string
	char tmp[LENGTH];
	for(int i = 0; i < TEMP_SENSORS; i++) {
		if(i == 0) {
			snprintf(str_cpu_temp, LENGTH, "%d°C ", cpu_temp[i]);
		} else {
			if(i == (TEMP_SENSORS - 1)) {
				snprintf(tmp, LENGTH, "%d°C", cpu_temp[i]);
			} else {
				snprintf(tmp, LENGTH, "%d°C ", cpu_temp[i]);	
			}
			strcat(str_cpu_temp, tmp);
		}
	}


	snprintf(status, WHOLE_LENGTH, 
			"%s < WLAN %s < Brightness %d%% < Volume %s < Ram %d%% < Temperature %s < Cpu %d%% < Battery %d%% (%s) < %s ", 
			kernel, 
			network_status, 
			brightness, 
			volume, 
			ram_usage,
			str_cpu_temp,
			cpu_usage,
			bat_capacity,
			bat_time,
			datetime
	);

	set_status(status);

	exit(EXIT_SUCCESS);
}
